// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/core_tab_helper.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "net/base/load_states.h"
#include "net/http/http_request_headers.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif

using content::WebContents;

namespace {

const int kImageSearchThumbnailMinSize = 300 * 300;
const int kImageSearchThumbnailMaxWidth = 600;
const int kImageSearchThumbnailMaxHeight = 600;

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(CoreTabHelper);

CoreTabHelper::CoreTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      delegate_(NULL),
      content_restrictions_(0) {
}

CoreTabHelper::~CoreTabHelper() {
}

base::string16 CoreTabHelper::GetDefaultTitle() {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
}

base::string16 CoreTabHelper::GetStatusText() const {
  base::string16 status_text;
  GetStatusTextForWebContents(&status_text, web_contents());
  return status_text;
}

void CoreTabHelper::OnCloseStarted() {
  if (close_start_time_.is_null())
    close_start_time_ = base::TimeTicks::Now();
}

void CoreTabHelper::OnCloseCanceled() {
  close_start_time_ = base::TimeTicks();
  before_unload_end_time_ = base::TimeTicks();
  unload_detached_start_time_ = base::TimeTicks();
}

void CoreTabHelper::OnUnloadStarted() {
  before_unload_end_time_ = base::TimeTicks::Now();
}

void CoreTabHelper::OnUnloadDetachedStarted() {
  if (unload_detached_start_time_.is_null())
    unload_detached_start_time_ = base::TimeTicks::Now();
}

void CoreTabHelper::UpdateContentRestrictions(int content_restrictions) {
  content_restrictions_ = content_restrictions;
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;

  browser->command_controller()->ContentRestrictionsChanged();
#endif
}

void CoreTabHelper::SearchByImageInNewTab(
    content::RenderFrameHost* render_frame_host, const GURL& src_url) {
  RequestThumbnailForContextNode(
      render_frame_host,
      kImageSearchThumbnailMinSize,
      gfx::Size(kImageSearchThumbnailMaxWidth,
                kImageSearchThumbnailMaxHeight),
      base::Bind(&CoreTabHelper::DoSearchByImageInNewTab,
                 base::Unretained(this),
                 src_url));
}

void CoreTabHelper::RequestThumbnailForContextNode(
    content::RenderFrameHost* render_frame_host,
    int minimum_size,
    gfx::Size maximum_size,
    const ContextNodeThumbnailCallback& callback) {
  int callback_id = thumbnail_callbacks_.Add(
      base::MakeUnique<ContextNodeThumbnailCallback>(callback));

  render_frame_host->Send(
      new ChromeViewMsg_RequestThumbnailForContextNode(
          render_frame_host->GetRoutingID(),
          minimum_size,
          maximum_size,
          callback_id));
}

// static
bool CoreTabHelper::GetStatusTextForWebContents(
    base::string16* status_text, content::WebContents* source) {
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/467185 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "467185 CoreTabHelper::GetStatusTextForWebContents1"));
  auto* guest_manager = guest_view::GuestViewManager::FromBrowserContext(
      source->GetBrowserContext());
  if (!source->IsLoading() ||
      source->GetLoadState().state == net::LOAD_STATE_IDLE) {
    // TODO(robliao): Remove ScopedTracker below once https://crbug.com/467185
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile2(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "467185 CoreTabHelper::GetStatusTextForWebContents2"));
    if (!guest_manager)
      return false;
    return guest_manager->ForEachGuest(
        source, base::Bind(&CoreTabHelper::GetStatusTextForWebContents,
                           status_text));
  }

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/467185
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "467185 CoreTabHelper::GetStatusTextForWebContents3"));

  switch (source->GetLoadState().state) {
    case net::LOAD_STATE_WAITING_FOR_STALLED_SOCKET_POOL:
    case net::LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET:
      *status_text =
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_SOCKET_SLOT);
      return true;
    case net::LOAD_STATE_WAITING_FOR_DELEGATE:
      if (!source->GetLoadState().param.empty()) {
        *status_text = l10n_util::GetStringFUTF16(
            IDS_LOAD_STATE_WAITING_FOR_DELEGATE,
            source->GetLoadState().param);
        return true;
      } else {
        *status_text = l10n_util::GetStringUTF16(
            IDS_LOAD_STATE_WAITING_FOR_DELEGATE_GENERIC);
        return true;
      }
    case net::LOAD_STATE_WAITING_FOR_CACHE:
      *status_text =
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_CACHE);
      return true;
    case net::LOAD_STATE_WAITING_FOR_APPCACHE:
      *status_text =
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_APPCACHE);
      return true;
    case net::LOAD_STATE_ESTABLISHING_PROXY_TUNNEL:
      *status_text =
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_ESTABLISHING_PROXY_TUNNEL);
      return true;
    case net::LOAD_STATE_DOWNLOADING_PROXY_SCRIPT:
      *status_text =
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_DOWNLOADING_PROXY_SCRIPT);
      return true;
    case net::LOAD_STATE_RESOLVING_PROXY_FOR_URL:
      *status_text =
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_PROXY_FOR_URL);
      return true;
    case net::LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT:
      *status_text = l10n_util::GetStringUTF16(
          IDS_LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT);
      return true;
    case net::LOAD_STATE_RESOLVING_HOST:
      *status_text = l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_HOST);
      return true;
    case net::LOAD_STATE_CONNECTING:
      *status_text = l10n_util::GetStringUTF16(IDS_LOAD_STATE_CONNECTING);
      return true;
    case net::LOAD_STATE_SSL_HANDSHAKE:
      *status_text = l10n_util::GetStringUTF16(IDS_LOAD_STATE_SSL_HANDSHAKE);
      return true;
    case net::LOAD_STATE_SENDING_REQUEST:
      if (source->GetUploadSize()) {
        *status_text = l10n_util::GetStringFUTF16Int(
            IDS_LOAD_STATE_SENDING_REQUEST_WITH_PROGRESS,
            static_cast<int>((100 * source->GetUploadPosition()) /
                source->GetUploadSize()));
        return true;
      } else {
        *status_text =
            l10n_util::GetStringUTF16(IDS_LOAD_STATE_SENDING_REQUEST);
        return true;
      }
    case net::LOAD_STATE_WAITING_FOR_RESPONSE:
      *status_text =
          l10n_util::GetStringFUTF16(IDS_LOAD_STATE_WAITING_FOR_RESPONSE,
                                     source->GetLoadStateHost());
      return true;
    case net::LOAD_STATE_THROTTLED:
      *status_text = l10n_util::GetStringUTF16(IDS_LOAD_STATE_THROTTLED);
      return true;
    // Ignore net::LOAD_STATE_READING_RESPONSE and net::LOAD_STATE_IDLE
    case net::LOAD_STATE_IDLE:
    case net::LOAD_STATE_READING_RESPONSE:
      break;
  }
  if (!guest_manager)
    return false;

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/467185 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile4(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "467185 CoreTabHelper::GetStatusTextForWebContents4"));
  return guest_manager->ForEachGuest(
      source, base::Bind(&CoreTabHelper::GetStatusTextForWebContents,
                         status_text));
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsObserver overrides

void CoreTabHelper::DidStartLoading() {
  UpdateContentRestrictions(0);
}

void CoreTabHelper::WasShown() {
  web_cache::WebCacheManager::GetInstance()->ObserveActivity(
      web_contents()->GetRenderProcessHost()->GetID());
}

void CoreTabHelper::WebContentsDestroyed() {
  // OnCloseStarted isn't called in unit tests.
  if (!close_start_time_.is_null()) {
    bool fast_tab_close_enabled =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableFastUnload);

    if (fast_tab_close_enabled) {
      base::TimeTicks now = base::TimeTicks::Now();
      base::TimeDelta close_time = now - close_start_time_;
      UMA_HISTOGRAM_TIMES("Tab.Close", close_time);

      base::TimeTicks unload_start_time = close_start_time_;
      base::TimeTicks unload_end_time = now;
      if (!before_unload_end_time_.is_null())
        unload_start_time = before_unload_end_time_;
      if (!unload_detached_start_time_.is_null())
        unload_end_time = unload_detached_start_time_;
      base::TimeDelta unload_time = unload_end_time - unload_start_time;
      UMA_HISTOGRAM_TIMES("Tab.Close.UnloadTime", unload_time);
    } else {
      base::TimeTicks now = base::TimeTicks::Now();
      base::TimeTicks unload_start_time = close_start_time_;
      if (!before_unload_end_time_.is_null())
        unload_start_time = before_unload_end_time_;
      UMA_HISTOGRAM_TIMES("Tab.Close", now - close_start_time_);
      UMA_HISTOGRAM_TIMES("Tab.Close.UnloadTime", now - unload_start_time);
    }
  }
}

void CoreTabHelper::BeforeUnloadFired(const base::TimeTicks& proceed_time) {
  before_unload_end_time_ = proceed_time;
}

void CoreTabHelper::BeforeUnloadDialogCancelled() {
  OnCloseCanceled();
}

bool CoreTabHelper::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CoreTabHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_RequestThumbnailForContextNode_ACK,
                        OnRequestThumbnailForContextNodeACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CoreTabHelper::OnRequestThumbnailForContextNodeACK(
    const std::string& thumbnail_data,
    const gfx::Size& original_size,
    int callback_id) {
  ContextNodeThumbnailCallback* callback =
      thumbnail_callbacks_.Lookup(callback_id);
  if (!callback)
    return;
  callback->Run(thumbnail_data, original_size);
  thumbnail_callbacks_.Remove(callback_id);
}

// Handles the image thumbnail for the context node, composes a image search
// request based on the received thumbnail and opens the request in a new tab.
void CoreTabHelper::DoSearchByImageInNewTab(const GURL& src_url,
                                            const std::string& thumbnail_data,
                                            const gfx::Size& original_size) {
  if (thumbnail_data.empty())
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (!template_url_service)
    return;
  const TemplateURL* const default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_provider)
    return;

  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(base::string16());
  search_args.image_thumbnail_content = thumbnail_data;
  search_args.image_url = src_url;
  search_args.image_original_size = original_size;
  TemplateURLRef::PostContent post_content;
  GURL result(default_provider->image_url_ref().ReplaceSearchTerms(
      search_args, template_url_service->search_terms_data(), &post_content));
  if (!result.is_valid())
    return;

  content::OpenURLParams open_url_params(
      result, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false);
  const std::string& content_type = post_content.first;
  const std::string& post_data = post_content.second;
  if (!post_data.empty()) {
    DCHECK(!content_type.empty());
    open_url_params.uses_post = true;
    open_url_params.post_data = content::ResourceRequestBody::CreateFromBytes(
        post_data.data(), post_data.size());
    open_url_params.extra_headers += base::StringPrintf(
        "%s: %s\r\n", net::HttpRequestHeaders::kContentType,
        content_type.c_str());
  }
  web_contents()->OpenURL(open_url_params);
}
