// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_helpers.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_factory.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tab_helper.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/navigation_correction_tab_observer.h"
#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/dom_distiller/content/web_contents_main_frame_observer.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/webapps/single_tab_mode_tab_helper.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#else
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/net/predictor_tab_helper.h"
#include "chrome/browser/plugins/plugin_observer.h"
#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"
#include "chrome/browser/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/hung_plugin_tab_helper.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
#include "chrome/browser/ui/metro_pin_tab_helper_win.h"
#endif

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "extensions/browser/view_type_utils.h"
#endif

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#endif

#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)

#if defined(ENABLE_ONE_CLICK_SIGNIN)
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#endif

using content::WebContents;

namespace {

const char kTabContentsAttachedTabHelpersUserDataKey[] =
    "TabContentsAttachedTabHelpers";

}  // namespace

// static
void TabHelpers::AttachTabHelpers(WebContents* web_contents) {
  // If already adopted, nothing to be done.
  base::SupportsUserData::Data* adoption_tag =
      web_contents->GetUserData(&kTabContentsAttachedTabHelpersUserDataKey);
  if (adoption_tag)
    return;

  // Mark as adopted.
  web_contents->SetUserData(&kTabContentsAttachedTabHelpersUserDataKey,
                            new base::SupportsUserData::Data());

#if defined(ENABLE_EXTENSIONS)
  // Set the view type.
  extensions::SetViewType(web_contents, extensions::VIEW_TYPE_TAB_CONTENTS);
#endif

  // Create all the tab helpers.

  // SessionTabHelper comes first because it sets up the tab ID, and other
  // helpers may rely on that.
  SessionTabHelper::CreateForWebContents(web_contents);
#if !defined(OS_ANDROID)
  // ZoomController comes before common tab helpers since ChromeAutofillClient
  // may want to register as a ZoomObserver with it.
  ZoomController::CreateForWebContents(web_contents);
#endif

  // --- Common tab helpers ---

  autofill::ChromeAutofillClient::CreateForWebContents(web_contents);
  autofill::ContentAutofillDriver::CreateForWebContentsAndDelegate(
      web_contents,
      autofill::ChromeAutofillClient::FromWebContents(web_contents),
      g_browser_process->GetApplicationLocale(),
      autofill::AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);
  BookmarkTabHelper::CreateForWebContents(web_contents);
  chrome_browser_net::NetErrorTabHelper::CreateForWebContents(web_contents);
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents,
      autofill::ChromeAutofillClient::FromWebContents(web_contents));
  CoreTabHelper::CreateForWebContents(web_contents);
  FaviconTabHelper::CreateForWebContents(web_contents);
  FindTabHelper::CreateForWebContents(web_contents);
  HistoryTabHelper::CreateForWebContents(web_contents);
  InfoBarService::CreateForWebContents(web_contents);
  NavigationCorrectionTabObserver::CreateForWebContents(web_contents);
  NavigationMetricsRecorder::CreateForWebContents(web_contents);
  PopupBlockerTabHelper::CreateForWebContents(web_contents);
  PrefsTabHelper::CreateForWebContents(web_contents);
  prerender::PrerenderTabHelper::CreateForWebContentsWithPasswordManager(
      web_contents,
      ChromePasswordManagerClient::GetManagerFromWebContents(web_contents));
  SearchTabHelper::CreateForWebContents(web_contents);
  TabSpecificContentSettings::CreateForWebContents(web_contents);
  ChromeTranslateClient::CreateForWebContents(web_contents);

  // --- Platform-specific tab helpers ---

#if defined(OS_ANDROID)
  ContextMenuHelper::CreateForWebContents(web_contents);
  SingleTabModeTabHelper::CreateForWebContents(web_contents);
  WindowAndroidHelper::CreateForWebContents(web_contents);
#else
  chrome_browser_net::PredictorTabHelper::CreateForWebContents(web_contents);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
  extensions::WebNavigationTabObserver::CreateForWebContents(web_contents);
  ExternalProtocolObserver::CreateForWebContents(web_contents);
  HungPluginTabHelper::CreateForWebContents(web_contents);
  ManagePasswordsUIController::CreateForWebContents(web_contents);
  pdf::PDFWebContentsHelper::CreateForWebContentsWithClient(
      web_contents,
      scoped_ptr<pdf::PDFWebContentsHelperClient>(
          new ChromePDFWebContentsHelperClient()));
  PermissionBubbleManager::CreateForWebContents(web_contents);
  PluginObserver::CreateForWebContents(web_contents);
  SadTabHelper::CreateForWebContents(web_contents);
  safe_browsing::SafeBrowsingTabObserver::CreateForWebContents(web_contents);
  SearchEngineTabHelper::CreateForWebContents(web_contents);
  TabContentsSyncedTabDelegate::CreateForWebContents(web_contents);
  ThumbnailTabHelper::CreateForWebContents(web_contents);
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);
#endif

#if defined(OS_WIN)
  MetroPinTabHelper::CreateForWebContents(web_contents);
#endif

  // --- Feature tab helpers behind flags ---

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalTabHelper::CreateForWebContents(web_contents);
#endif

#if defined(ENABLE_EXTENSIONS)
  extensions::TabHelper::CreateForWebContents(web_contents);
#endif

#if defined(ENABLE_MANAGED_USERS)
  SupervisedUserNavigationObserver::CreateForWebContents(web_contents);
#endif

#if defined(ENABLE_PRINTING) && !defined(OS_ANDROID)
#if defined(ENABLE_FULL_PRINTING)
  printing::PrintViewManager::CreateForWebContents(web_contents);
  printing::PrintPreviewMessageHandler::CreateForWebContents(web_contents);
#else
  printing::PrintViewManagerBasic::CreateForWebContents(web_contents);
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING) && !defined(OS_ANDROID)

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableDomDistiller)) {
    dom_distiller::WebContentsMainFrameObserver::CreateForWebContents(
        web_contents);
  }

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  // If this is not an incognito window, setup to handle one-click login.
  // We don't want to check that the profile is already connected at this time
  // because the connected state may change while this tab is open.  Having a
  // one-click signin helper attached does not cause problems if the profile
  // happens to be already connected.
  if (OneClickSigninHelper::CanOffer(web_contents,
                                     OneClickSigninHelper::CAN_OFFER_FOR_ALL,
                                     std::string(),
                                     NULL)) {
    OneClickSigninHelper::CreateForWebContentsWithPasswordManager(
        web_contents,
        ChromePasswordManagerClient::GetManagerFromWebContents(web_contents));
  }
#endif

  if (predictors::ResourcePrefetchPredictorFactory::GetForProfile(
      web_contents->GetBrowserContext())) {
    predictors::ResourcePrefetchPredictorTabHelper::CreateForWebContents(
        web_contents);
  }
}
