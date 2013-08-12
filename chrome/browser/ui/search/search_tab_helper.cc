// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SearchTabHelper);

namespace {

bool IsNTP(const content::WebContents* contents) {
  // We can't use WebContents::GetURL() because that uses the active entry,
  // whereas we want the visible entry.
  const content::NavigationEntry* entry =
      contents->GetController().GetVisibleEntry();
  if (entry && entry->GetVirtualURL() == GURL(chrome::kChromeUINewTabURL))
    return true;

  return chrome::IsInstantNTP(contents);
}

bool IsSearchResults(const content::WebContents* contents) {
  return !chrome::GetSearchTerms(contents).empty();
}

// TODO(kmadhusu): Move this helper from anonymous namespace to chrome
// namespace and remove InstantPage::IsLocal().
bool IsLocal(const content::WebContents* contents) {
  return contents &&
      contents->GetURL() == GURL(chrome::kChromeSearchLocalNtpUrl);
}

// Returns true if |contents| are rendered inside an Instant process.
bool InInstantProcess(Profile* profile,
                      const content::WebContents* contents) {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  return instant_service &&
      instant_service->IsInstantProcess(
          contents->GetRenderProcessHost()->GetID());
}

// Updates the location bar to reflect |contents| Instant support state.
void UpdateLocationBar(content::WebContents* contents) {
// iOS and Android doesn't use the Instant framework.
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  if (!contents)
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (!browser)
    return;
  browser->OnWebContentsInstantSupportDisabled(contents);
#endif
}

}  // namespace

SearchTabHelper::SearchTabHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      is_search_enabled_(chrome::IsInstantExtendedAPIEnabled()),
      user_input_in_progress_(false),
      web_contents_(web_contents) {
  if (!is_search_enabled_)
    return;

  // Observe NOTIFICATION_NAV_ENTRY_COMMITTED events so we can reset state
  // associated with the WebContents  (such as mode, last known most visited
  // items, instant support state etc).
  registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(
          &web_contents->GetController()));
}

SearchTabHelper::~SearchTabHelper() {
}

void SearchTabHelper::InitForPreloadedNTP() {
  UpdateMode(true, true);
}

void SearchTabHelper::OmniboxEditModelChanged(bool user_input_in_progress,
                                              bool cancelling) {
  if (!is_search_enabled_)
    return;

  user_input_in_progress_ = user_input_in_progress;
  if (!user_input_in_progress && !cancelling)
    return;

  UpdateMode(false, false);
}

void SearchTabHelper::NavigationEntryUpdated() {
  if (!is_search_enabled_)
    return;

  UpdateMode(false, false);
}

void SearchTabHelper::InstantSupportChanged(bool instant_support) {
  if (!is_search_enabled_)
    return;

  InstantSupportState new_state = instant_support ? INSTANT_SUPPORT_YES :
      INSTANT_SUPPORT_NO;

  model_.SetInstantSupportState(new_state);

  content::NavigationEntry* entry =
      web_contents_->GetController().GetVisibleEntry();
  if (entry) {
    chrome::SetInstantSupportStateInNavigationEntry(new_state, entry);
    if (!instant_support)
      UpdateLocationBar(web_contents_);
  }
}

bool SearchTabHelper::SupportsInstant() const {
  return model_.instant_support() == INSTANT_SUPPORT_YES;
}

void SearchTabHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
  content::LoadCommittedDetails* load_details =
      content::Details<content::LoadCommittedDetails>(details).ptr();
  if (!load_details->is_main_frame)
    return;

  UpdateMode(true, false);

  content::NavigationEntry* entry =
      web_contents_->GetController().GetVisibleEntry();
  DCHECK(entry);

  // Already determined the instant support state for this page, do not reset
  // the instant support state.
  //
  // When we get a navigation entry committed event, there seem to be two ways
  // to tell whether the navigation was "in-page". Ideally, when
  // LoadCommittedDetails::is_in_page is true, we should have
  // LoadCommittedDetails::type to be NAVIGATION_TYPE_IN_PAGE. Unfortunately,
  // they are different in some cases. To workaround this bug, we are checking
  // (is_in_page || type == NAVIGATION_TYPE_IN_PAGE). Please refer to
  // crbug.com/251330 for more details.
  if (load_details->is_in_page ||
      load_details->type == content::NAVIGATION_TYPE_IN_PAGE) {
    // When an "in-page" navigation happens, we will not receive a
    // DidFinishLoad() event. Therefore, we will not determine the Instant
    // support for the navigated page. So, copy over the Instant support from
    // the previous entry. If the page does not support Instant, update the
    // location bar from here to turn off search terms replacement.
    chrome::SetInstantSupportStateInNavigationEntry(model_.instant_support(),
                                                    entry);
    if (model_.instant_support() == INSTANT_SUPPORT_NO)
      UpdateLocationBar(web_contents_);
    return;
  }

  model_.SetInstantSupportState(INSTANT_SUPPORT_UNKNOWN);
  model_.SetVoiceSearchSupported(false);
  chrome::SetInstantSupportStateInNavigationEntry(model_.instant_support(),
                                                  entry);
}

void SearchTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Always set the title on the new tab page to be the one from our UI
  // resources.  Normally, we set the title when we begin a NTP load, but it
  // can get reset in several places (like when you press Reload). This check
  // ensures that the title is properly set to the string defined by the Chrome
  // UI language (rather than the server language) in all cases.
  //
  // We only override the title when it's nonempty to allow the page to set the
  // title if it really wants. An empty title means to use the default. There's
  // also a race condition between this code and the page's SetTitle call which
  // this rule avoids.
  content::NavigationEntry* entry =
      web_contents_->GetController().GetActiveEntry();
  if (entry && entry->GetTitle().empty() &&
      (entry->GetVirtualURL() == GURL(chrome::kChromeUINewTabURL) ||
       chrome::NavEntryIsInstantNTP(web_contents_, entry))) {
    entry->SetTitle(l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  }
}

bool SearchTabHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SearchTabHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_InstantSupportDetermined,
                        OnInstantSupportDetermined)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SetVoiceSearchSupported,
                        OnSetVoiceSearchSupported)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SearchTabHelper::DidFinishLoad(
    int64 /* frame_id */,
    const GURL&  /* validated_url */,
    bool is_main_frame,
    content::RenderViewHost* /* render_view_host */) {
  if (is_main_frame)
    DetermineIfPageSupportsInstant();
}

void SearchTabHelper::UpdateMode(bool update_origin, bool is_preloaded_ntp) {
  SearchMode::Type type = SearchMode::MODE_DEFAULT;
  SearchMode::Origin origin = SearchMode::ORIGIN_DEFAULT;
  if (IsNTP(web_contents_) || is_preloaded_ntp) {
    type = SearchMode::MODE_NTP;
    origin = SearchMode::ORIGIN_NTP;
  } else if (IsSearchResults(web_contents_)) {
    type = SearchMode::MODE_SEARCH_RESULTS;
    origin = SearchMode::ORIGIN_SEARCH;
  }
  if (!update_origin)
    origin = model_.mode().origin;
  if (user_input_in_progress_)
    type = SearchMode::MODE_SEARCH_SUGGESTIONS;
  model_.SetMode(SearchMode(type, origin));
}

void SearchTabHelper::DetermineIfPageSupportsInstant() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (!InInstantProcess(profile, web_contents_)) {
    // The page is not in the Instant process. This page does not support
    // instant. If we send an IPC message to a page that is not in the Instant
    // process, it will never receive it and will never respond. Therefore,
    // return immediately.
    InstantSupportChanged(false);
  } else if (IsLocal(web_contents_)) {
    // Local pages always support Instant.
    InstantSupportChanged(true);
  } else {
    Send(new ChromeViewMsg_DetermineIfPageSupportsInstant(routing_id()));
  }
}

void SearchTabHelper::OnInstantSupportDetermined(int page_id,
                                                 bool instant_support) {
  if (!web_contents()->IsActiveEntry(page_id))
    return;

  InstantSupportChanged(instant_support);
}

void SearchTabHelper::OnSetVoiceSearchSupported(int page_id, bool supported) {
  if (web_contents()->IsActiveEntry(page_id))
    model_.SetVoiceSearchSupported(supported);
}
