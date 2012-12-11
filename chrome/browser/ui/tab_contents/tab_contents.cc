// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/automation/automation_tab_helper.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"
#include "chrome/browser/net/load_time_stats.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/omnibox_search_hint.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_manager_delegate_impl.h"
#include "chrome/browser/plugins/plugin_observer.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ssl/ssl_tab_helper.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/three_d_api_observer.h"
#include "chrome/browser/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/alternate_error_tab_observer.h"
#include "chrome/browser/ui/autofill/tab_autofill_manager_delegate.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/hung_plugin_tab_helper.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/pdf/pdf_tab_helper.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/snapshot_tab_helper.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/metro_pin_tab_helper_win.h"
#endif

using content::WebContents;

namespace {

const char kTabContentsUserDataKey[] = "TabContentsUserData";

class TabContentsUserData : public base::SupportsUserData::Data {
 public:
  explicit TabContentsUserData(TabContents* tab_contents)
      : tab_contents_(tab_contents) {}
  virtual ~TabContentsUserData() {}
  TabContents* tab_contents() { return tab_contents_; }

  void MakeContentsOwned() { owned_tab_contents_.reset(tab_contents_); }

 private:
  TabContents* tab_contents_;  // unowned
  scoped_ptr<TabContents> owned_tab_contents_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TabContents, public:

// static
TabContents* TabContents::Factory::CreateTabContents(WebContents* contents) {
  return new TabContents(contents);
}

// static
TabContents::TabContents(WebContents* contents)
    : content::WebContentsObserver(contents),
      in_destructor_(false),
      web_contents_(contents),
      profile_(Profile::FromBrowserContext(contents->GetBrowserContext())),
      owned_web_contents_(contents) {
  DCHECK(contents);
  DCHECK(!FromWebContents(contents));

  chrome::SetViewType(contents, chrome::VIEW_TYPE_TAB_CONTENTS);

  // Stash this in the WebContents.
  contents->SetUserData(&kTabContentsUserDataKey,
                        new TabContentsUserData(this));

  // Create the tab helpers.

  // SessionTabHelper comes first because it sets up the tab ID, and other
  // helpers may rely on that.
  SessionTabHelper::CreateForWebContents(contents);

  AlternateErrorPageTabObserver::CreateForWebContents(contents);
  TabAutofillManagerDelegate::CreateForWebContents(contents);
  AutofillManager::CreateForWebContentsAndDelegate(
      contents, TabAutofillManagerDelegate::FromWebContents(contents));
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNewAutofillUi)) {
    AutofillExternalDelegate::CreateForWebContentsAndManager(
        contents, AutofillManager::FromWebContents(contents));
    AutofillManager::FromWebContents(contents)->SetExternalDelegate(
        AutofillExternalDelegate::FromWebContents(contents));
  }
  BlockedContentTabHelper::CreateForWebContents(contents);
  BookmarkTabHelper::CreateForWebContents(contents);
  chrome_browser_net::LoadTimeStatsTabHelper::CreateForWebContents(contents);
  chrome_browser_net::NetErrorTabHelper::CreateForWebContents(contents);
  ConstrainedWindowTabHelper::CreateForWebContents(contents);
  CoreTabHelper::CreateForWebContents(contents);
  extensions::TabHelper::CreateForWebContents(contents);
  extensions::WebNavigationTabObserver::CreateForWebContents(contents);
  ExternalProtocolObserver::CreateForWebContents(contents);
  FaviconTabHelper::CreateForWebContents(contents);
  FindTabHelper::CreateForWebContents(contents);
  HistoryTabHelper::CreateForWebContents(contents);
  HungPluginTabHelper::CreateForWebContents(contents);
  InfoBarTabHelper::CreateForWebContents(contents);
  NavigationMetricsRecorder::CreateForWebContents(contents);
  PasswordManagerDelegateImpl::CreateForWebContents(contents);
  PasswordManager::CreateForWebContentsAndDelegate(
      contents, PasswordManagerDelegateImpl::FromWebContents(contents));
  PluginObserver::CreateForWebContents(contents);
  PrefsTabHelper::CreateForWebContents(contents);
  prerender::PrerenderTabHelper::CreateForWebContents(contents);
  safe_browsing::SafeBrowsingTabObserver::CreateForWebContents(contents);
  SearchEngineTabHelper::CreateForWebContents(contents);
  chrome::search::SearchTabHelper::CreateForWebContents(contents);
  SnapshotTabHelper::CreateForWebContents(contents);
  SSLTabHelper::CreateForWebContents(contents);
  TabContentsSyncedTabDelegate::CreateForWebContents(contents);
  TabSpecificContentSettings::CreateForWebContents(contents);
  ThreeDAPIObserver::CreateForWebContents(contents);
  ThumbnailTabHelper::CreateForWebContents(contents);
  TranslateTabHelper::CreateForWebContents(contents);
  ZoomController::CreateForWebContents(contents);

#if defined(ENABLE_AUTOMATION)
  AutomationTabHelper::CreateForWebContents(contents);
#endif

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalTabHelper::CreateForWebContents(contents);
#endif

#if !defined(OS_ANDROID)
  if (OmniboxSearchHint::IsEnabled(profile()))
    OmniboxSearchHint::CreateForWebContents(contents);
  ManagedModeNavigationObserver::CreateForWebContents(contents);
  PDFTabHelper::CreateForWebContents(contents);
  SadTabHelper::CreateForWebContents(contents);
  WebIntentPickerController::CreateForWebContents(contents);
#endif

#if defined(ENABLE_PRINTING)
  printing::PrintPreviewMessageHandler::CreateForWebContents(contents);
  printing::PrintViewManager::CreateForWebContents(contents);
#endif

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  // If this is not an incognito window, setup to handle one-click login.
  // We don't want to check that the profile is already connected at this time
  // because the connected state may change while this tab is open.  Having a
  // one-click signin helper attached does not cause problems if the profile
  // happens to be already connected.
  if (OneClickSigninHelper::CanOffer(contents,
          OneClickSigninHelper::CAN_OFFER_FOR_ALL, "", NULL))
    OneClickSigninHelper::CreateForWebContents(contents);
#endif

#if defined(OS_WIN)
  MetroPinTabHelper::CreateForWebContents(contents);
#endif
}

TabContents::~TabContents() {
  in_destructor_ = true;
}

// static
TabContents* TabContents::FromWebContents(WebContents* contents) {
  TabContentsUserData* user_data = static_cast<TabContentsUserData*>(
      contents->GetUserData(&kTabContentsUserDataKey));

  return user_data ? user_data->tab_contents() : NULL;
}

// static
const TabContents* TabContents::FromWebContents(const WebContents* contents) {
  TabContentsUserData* user_data = static_cast<TabContentsUserData*>(
      contents->GetUserData(&kTabContentsUserDataKey));

  return user_data ? user_data->tab_contents() : NULL;
}

WebContents* TabContents::web_contents() const {
  return web_contents_;
}

Profile* TabContents::profile() const {
  return profile_;
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsObserver overrides

void TabContents::WebContentsDestroyed(WebContents* tab) {
  if (!in_destructor_) {
    // The owned WebContents is being destroyed independently, so delete this.
    ignore_result(owned_web_contents_.release());
    TabContentsUserData* user_data = static_cast<TabContentsUserData*>(
        tab->GetUserData(&kTabContentsUserDataKey));
    user_data->MakeContentsOwned();
  }
}
