// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/core_tab_helper.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_states.h"
#include "net/http/http_request_headers.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/jpeg_codec.h"

using content::WebContents;

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
  if (!web_contents()->IsLoading() ||
      web_contents()->GetLoadState().state == net::LOAD_STATE_IDLE) {
    return base::string16();
  }

  switch (web_contents()->GetLoadState().state) {
    case net::LOAD_STATE_WAITING_FOR_STALLED_SOCKET_POOL:
    case net::LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_SOCKET_SLOT);
    case net::LOAD_STATE_WAITING_FOR_DELEGATE:
      if (!web_contents()->GetLoadState().param.empty()) {
        return l10n_util::GetStringFUTF16(IDS_LOAD_STATE_WAITING_FOR_DELEGATE,
                                          web_contents()->GetLoadState().param);
      } else {
        return l10n_util::GetStringUTF16(
            IDS_LOAD_STATE_WAITING_FOR_DELEGATE_GENERIC);
      }
    case net::LOAD_STATE_WAITING_FOR_CACHE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_CACHE);
    case net::LOAD_STATE_WAITING_FOR_APPCACHE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_APPCACHE);
    case net::LOAD_STATE_ESTABLISHING_PROXY_TUNNEL:
      return
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_ESTABLISHING_PROXY_TUNNEL);
    case net::LOAD_STATE_DOWNLOADING_PROXY_SCRIPT:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_DOWNLOADING_PROXY_SCRIPT);
    case net::LOAD_STATE_RESOLVING_PROXY_FOR_URL:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_PROXY_FOR_URL);
    case net::LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT:
      return l10n_util::GetStringUTF16(
          IDS_LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT);
    case net::LOAD_STATE_RESOLVING_HOST:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_HOST);
    case net::LOAD_STATE_CONNECTING:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_CONNECTING);
    case net::LOAD_STATE_SSL_HANDSHAKE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_SSL_HANDSHAKE);
    case net::LOAD_STATE_SENDING_REQUEST:
      if (web_contents()->GetUploadSize()) {
        return l10n_util::GetStringFUTF16Int(
            IDS_LOAD_STATE_SENDING_REQUEST_WITH_PROGRESS,
            static_cast<int>((100 * web_contents()->GetUploadPosition()) /
                web_contents()->GetUploadSize()));
      } else {
        return l10n_util::GetStringUTF16(IDS_LOAD_STATE_SENDING_REQUEST);
      }
    case net::LOAD_STATE_WAITING_FOR_RESPONSE:
      return l10n_util::GetStringFUTF16(IDS_LOAD_STATE_WAITING_FOR_RESPONSE,
                                        web_contents()->GetLoadStateHost());
    // Ignore net::LOAD_STATE_READING_RESPONSE and net::LOAD_STATE_IDLE
    case net::LOAD_STATE_IDLE:
    case net::LOAD_STATE_READING_RESPONSE:
      break;
  }

  return base::string16();
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

////////////////////////////////////////////////////////////////////////////////
// WebContentsObserver overrides

void CoreTabHelper::DidStartLoading(content::RenderViewHost* render_view_host) {
  UpdateContentRestrictions(0);
}

void CoreTabHelper::WasShown() {
  web_cache::WebCacheManager::GetInstance()->ObserveActivity(
      web_contents()->GetRenderProcessHost()->GetID());
}

void CoreTabHelper::WebContentsDestroyed() {
  // OnCloseStarted isn't called in unit tests.
  if (!close_start_time_.is_null()) {
    bool fast_tab_close_enabled = CommandLine::ForCurrentProcess()->HasSwitch(
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

// Handles the image thumbnail for the context node, composes a image search
// request based on the received thumbnail and opens the request in a new tab.
void CoreTabHelper::OnRequestThumbnailForContextNodeACK(
    const SkBitmap& bitmap,
    const gfx::Size& original_size) {
  if (bitmap.isNull())
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

  const int kDefaultQualityForImageSearch = 90;
  std::vector<unsigned char> data;
  if (!gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_SkBitmap, bitmap.width(), bitmap.height(),
      static_cast<int>(bitmap.rowBytes()), kDefaultQualityForImageSearch,
      &data))
    return;

  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(base::string16());
  search_args.image_thumbnail_content = std::string(data.begin(), data.end());
  // TODO(jnd): Add a method in WebContentsViewDelegate to get the image URL
  // from the ContextMenuParams which creates current context menu.
  search_args.image_url = GURL();
  search_args.image_original_size = original_size;
  TemplateURLRef::PostContent post_content;
  GURL result(default_provider->image_url_ref().ReplaceSearchTerms(
      search_args, template_url_service->search_terms_data(), &post_content));
  if (!result.is_valid())
    return;

  content::OpenURLParams open_url_params(
      result, content::Referrer(), NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false);
  const std::string& content_type = post_content.first;
  std::string* post_data = &post_content.second;
  if (!post_data->empty()) {
    DCHECK(!content_type.empty());
    open_url_params.uses_post = true;
    open_url_params.browser_initiated_post_data =
        base::RefCountedString::TakeString(post_data);
    open_url_params.extra_headers += base::StringPrintf(
        "%s: %s\r\n", net::HttpRequestHeaders::kContentType,
        content_type.c_str());
  }
  web_contents()->OpenURL(open_url_params);
}
