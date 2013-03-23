// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_contents.h"

#include "base/command_line.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"
#include "chrome/browser/net/load_time_stats.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/omnibox_search_hint.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_manager_delegate_impl.h"
#include "chrome/browser/plugins/plugin_observer.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ssl/ssl_tab_helper.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/alternate_error_tab_observer.h"
#include "chrome/browser/ui/autofill/tab_autofill_manager_delegate.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/hung_plugin_tab_helper.h"
#include "chrome/browser/ui/pdf/pdf_tab_helper.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/chrome_switches.h"
#include "components/autofill/browser/autofill_external_delegate.h"
#include "components/autofill/browser/autofill_manager.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_AUTOMATION)
#include "chrome/browser/automation/automation_tab_helper.h"
#endif

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#endif

#if defined(ENABLE_PRINTING)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#endif

#if defined(ENABLE_ONE_CLICK_SIGNIN)
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/metro_pin_tab_helper_win.h"
#endif

using content::WebContents;

namespace {

const char kTabContentsAttachedTabHelpersUserDataKey[] =
    "TabContentsAttachedTabHelpers";

}  // namespace

// static
void BrowserTabContents::AttachTabHelpers(WebContents* web_contents) {
  // If already adopted, nothing to be done.
  base::SupportsUserData::Data* adoption_tag =
      web_contents->GetUserData(&kTabContentsAttachedTabHelpersUserDataKey);
  if (adoption_tag)
    return;

  // Mark as adopted.
  web_contents->SetUserData(&kTabContentsAttachedTabHelpersUserDataKey,
                            new base::SupportsUserData::Data());

  // Set the view type.
  chrome::SetViewType(web_contents, chrome::VIEW_TYPE_TAB_CONTENTS);

  // Create all the tab helpers.

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // SessionTabHelper comes first because it sets up the tab ID, and other
  // helpers may rely on that.
  SessionTabHelper::CreateForWebContents(web_contents);

  AlternateErrorPageTabObserver::CreateForWebContents(web_contents);
  autofill::TabAutofillManagerDelegate::CreateForWebContents(web_contents);
  AutofillManager::CreateForWebContentsAndDelegate(
      web_contents,
      autofill::TabAutofillManagerDelegate::FromWebContents(web_contents));
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNativeAutofillUi)) {
    AutofillExternalDelegate::CreateForWebContentsAndManager(
        web_contents, AutofillManager::FromWebContents(web_contents));
    AutofillManager::FromWebContents(web_contents)->SetExternalDelegate(
        AutofillExternalDelegate::FromWebContents(web_contents));
  }
  BlockedContentTabHelper::CreateForWebContents(web_contents);
  BookmarkTabHelper::CreateForWebContents(web_contents);
  chrome_browser_net::LoadTimeStatsTabHelper::CreateForWebContents(
      web_contents);
  chrome_browser_net::NetErrorTabHelper::CreateForWebContents(web_contents);
  WebContentsModalDialogManager::CreateForWebContents(web_contents);
  CoreTabHelper::CreateForWebContents(web_contents);
  extensions::TabHelper::CreateForWebContents(web_contents);
  extensions::WebNavigationTabObserver::CreateForWebContents(web_contents);
  ExternalProtocolObserver::CreateForWebContents(web_contents);
  FaviconTabHelper::CreateForWebContents(web_contents);
  FindTabHelper::CreateForWebContents(web_contents);
  HistoryTabHelper::CreateForWebContents(web_contents);
  HungPluginTabHelper::CreateForWebContents(web_contents);
  InfoBarService::CreateForWebContents(web_contents);
  ManagedModeNavigationObserver::CreateForWebContents(web_contents);
  NavigationMetricsRecorder::CreateForWebContents(web_contents);
  if (OmniboxSearchHint::IsEnabled(profile))
    OmniboxSearchHint::CreateForWebContents(web_contents);
  PasswordManagerDelegateImpl::CreateForWebContents(web_contents);
  PasswordManager::CreateForWebContentsAndDelegate(
      web_contents, PasswordManagerDelegateImpl::FromWebContents(web_contents));
  PDFTabHelper::CreateForWebContents(web_contents);
  PluginObserver::CreateForWebContents(web_contents);
  PrefsTabHelper::CreateForWebContents(web_contents);
  prerender::PrerenderTabHelper::CreateForWebContents(web_contents);
  SadTabHelper::CreateForWebContents(web_contents);
  safe_browsing::SafeBrowsingTabObserver::CreateForWebContents(web_contents);
  SearchEngineTabHelper::CreateForWebContents(web_contents);
  chrome::search::SearchTabHelper::CreateForWebContents(web_contents);
  SSLTabHelper::CreateForWebContents(web_contents);
  TabContentsSyncedTabDelegate::CreateForWebContents(web_contents);
  TabSpecificContentSettings::CreateForWebContents(web_contents);
  ThumbnailTabHelper::CreateForWebContents(web_contents);
  TranslateTabHelper::CreateForWebContents(web_contents);
  ZoomController::CreateForWebContents(web_contents);

#if defined(ENABLE_AUTOMATION)
  AutomationTabHelper::CreateForWebContents(web_contents);
#endif

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalTabHelper::CreateForWebContents(web_contents);
#endif

#if defined(ENABLE_PRINTING)
  printing::PrintPreviewMessageHandler::CreateForWebContents(web_contents);
  printing::PrintViewManager::CreateForWebContents(web_contents);
#endif

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  // If this is not an incognito window, setup to handle one-click login.
  // We don't want to check that the profile is already connected at this time
  // because the connected state may change while this tab is open.  Having a
  // one-click signin helper attached does not cause problems if the profile
  // happens to be already connected.
  if (OneClickSigninHelper::CanOffer(
          web_contents, OneClickSigninHelper::CAN_OFFER_FOR_ALL, "", NULL)) {
    OneClickSigninHelper::CreateForWebContents(web_contents);
  }
#endif

#if defined(OS_WIN)
  MetroPinTabHelper::CreateForWebContents(web_contents);
#endif
}
