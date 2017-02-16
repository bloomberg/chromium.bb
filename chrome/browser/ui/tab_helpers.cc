// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_helpers.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/data_use_measurement/data_use_web_contents_observer.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_observer.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/net/predictor_tab_helper.h"
#include "chrome/browser/ntp_snippets/bookmark_last_visit_updater.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_factory.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tab_helper.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/previews/previews_infobar_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/tracing/navigation_tracing.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/navigation_correction_tab_observer.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/dom_distiller/content/browser/web_contents_main_frame_observer.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/history/content/browser/web_contents_top_sites_observer.h"
#include "components/history/core/browser/top_sites.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "extensions/features/features.h"
#include "printing/features/features.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/banners/app_banner_manager_android.h"
#include "chrome/browser/android/data_usage/data_use_tab_helper.h"
#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/android/offline_pages/recent_tab_helper.h"
#include "chrome/browser/android/search_geolocation/search_geolocation_disclosure_tab_helper.h"
#include "chrome/browser/android/voice_search_tab_helper.h"
#include "chrome/browser/android/webapps/single_tab_mode_tab_helper.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#else
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/plugins/plugin_observer.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer.h"
#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"
#include "chrome/browser/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/hung_plugin_tab_helper.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/zoom/zoom_controller.h"
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "extensions/browser/view_type_utils.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)

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

#if BUILDFLAG(ENABLE_EXTENSIONS)
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
  zoom::ZoomController::CreateForWebContents(web_contents);
#endif

  // --- Common tab helpers ---

  autofill::ChromeAutofillClient::CreateForWebContents(web_contents);
  autofill::ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
      web_contents,
      autofill::ChromeAutofillClient::FromWebContents(web_contents),
      g_browser_process->GetApplicationLocale(),
      autofill::AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);
  BookmarkLastVisitUpdater::MaybeCreateForWebContentsWithBookmarkModel(
      web_contents, BookmarkModelFactory::GetForBrowserContext(
                        web_contents->GetBrowserContext()));
  chrome_browser_net::NetErrorTabHelper::CreateForWebContents(web_contents);
  chrome_browser_net::PredictorTabHelper::CreateForWebContents(web_contents);
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents,
      autofill::ChromeAutofillClient::FromWebContents(web_contents));
  ChromeTranslateClient::CreateForWebContents(web_contents);
  CoreTabHelper::CreateForWebContents(web_contents);
  data_use_measurement::DataUseWebContentsObserver::CreateForWebContents(
      web_contents);
  ExternalProtocolObserver::CreateForWebContents(web_contents);
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
  FindTabHelper::CreateForWebContents(web_contents);
  history::WebContentsTopSitesObserver::CreateForWebContents(
      web_contents, TopSitesFactory::GetForProfile(
                        Profile::FromBrowserContext(
                            web_contents->GetBrowserContext())).get());
  HistoryTabHelper::CreateForWebContents(web_contents);
  InfoBarService::CreateForWebContents(web_contents);
  NavigationCorrectionTabObserver::CreateForWebContents(web_contents);
  NavigationMetricsRecorder::CreateForWebContents(web_contents);
  chrome::InitializePageLoadMetricsForWebContents(web_contents);
  PermissionRequestManager::CreateForWebContents(web_contents);
  PopupBlockerTabHelper::CreateForWebContents(web_contents);
  PrefsTabHelper::CreateForWebContents(web_contents);
  prerender::PrerenderTabHelper::CreateForWebContents(web_contents);
  PreviewsInfoBarTabHelper::CreateForWebContents(web_contents);
  SearchTabHelper::CreateForWebContents(web_contents);
  SearchEngineTabHelper::CreateForWebContents(web_contents);
  SecurityStateTabHelper::CreateForWebContents(web_contents);
  if (SiteEngagementService::IsEnabled())
    SiteEngagementService::Helper::CreateForWebContents(web_contents);
  std::unique_ptr<ChromeSubresourceFilterClient> subresource_filter_client(
      new ChromeSubresourceFilterClient(web_contents));
  subresource_filter::ContentSubresourceFilterDriverFactory::
      CreateForWebContents(web_contents, std::move(subresource_filter_client));
  // TODO(vabr): Remove TabSpecificContentSettings from here once their function
  // is taken over by ChromeContentSettingsClient. http://crbug.com/387075
  TabSpecificContentSettings::CreateForWebContents(web_contents);
  if (content::IsBrowserSideNavigationEnabled())
    MixedContentSettingsTabHelper::CreateForWebContents(web_contents);

  // --- Platform-specific tab helpers ---

#if defined(OS_ANDROID)
  banners::AppBannerManagerAndroid::CreateForWebContents(web_contents);
  ContextMenuHelper::CreateForWebContents(web_contents);
  DataUseTabHelper::CreateForWebContents(web_contents);

  offline_pages::OfflinePageTabHelper::CreateForWebContents(web_contents);
  offline_pages::RecentTabHelper::CreateForWebContents(web_contents);

  SearchGeolocationDisclosureTabHelper::CreateForWebContents(web_contents);
  SingleTabModeTabHelper::CreateForWebContents(web_contents);
  ViewAndroidHelper::CreateForWebContents(web_contents);
  VoiceSearchTabHelper::CreateForWebContents(web_contents);
#else
  BookmarkTabHelper::CreateForWebContents(web_contents);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
  extensions::WebNavigationTabObserver::CreateForWebContents(web_contents);
  HungPluginTabHelper::CreateForWebContents(web_contents);
  JavaScriptDialogTabHelper::CreateForWebContents(web_contents);
  ManagePasswordsUIController::CreateForWebContents(web_contents);
  pdf::PDFWebContentsHelper::CreateForWebContentsWithClient(
      web_contents, std::unique_ptr<pdf::PDFWebContentsHelperClient>(
                        new ChromePDFWebContentsHelperClient()));
  PluginObserver::CreateForWebContents(web_contents);
  SadTabHelper::CreateForWebContents(web_contents);
  safe_browsing::SafeBrowsingTabObserver::CreateForWebContents(web_contents);
  safe_browsing::SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
      web_contents);
  TabContentsSyncedTabDelegate::CreateForWebContents(web_contents);
  TabDialogs::CreateForWebContents(web_contents);
  ThumbnailTabHelper::CreateForWebContents(web_contents);
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);

  if (banners::AppBannerManagerDesktop::IsEnabled())
    banners::AppBannerManagerDesktop::CreateForWebContents(web_contents);
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  metrics::DesktopSessionDurationObserver::CreateForWebContents(web_contents);
#endif
// --- Feature tab helpers behind flags ---

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalTabHelper::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::TabHelper::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  SupervisedUserNavigationObserver::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_PRINTING) && !defined(OS_ANDROID)
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  printing::PrintViewManager::CreateForWebContents(web_contents);
  printing::PrintPreviewMessageHandler::CreateForWebContents(web_contents);
#else
  printing::PrintViewManagerBasic::CreateForWebContents(web_contents);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING) && !defined(OS_ANDROID)

  bool enabled_distiller = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDomDistiller);
  if (enabled_distiller) {
    dom_distiller::WebContentsMainFrameObserver::CreateForWebContents(
        web_contents);
  }

  if (predictors::ResourcePrefetchPredictorFactory::GetForProfile(
      web_contents->GetBrowserContext())) {
    predictors::ResourcePrefetchPredictorTabHelper::CreateForWebContents(
        web_contents);
  }

  if (tracing::NavigationTracingObserver::IsEnabled())
    tracing::NavigationTracingObserver::CreateForWebContents(web_contents);
}
