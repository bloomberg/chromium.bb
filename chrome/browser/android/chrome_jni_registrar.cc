// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/after_startup_task_utils_android.h"
#include "chrome/browser/android/accessibility/font_size_prefs_android.h"
#include "chrome/browser/android/banners/app_banner_infobar_delegate_android.h"
#include "chrome/browser/android/banners/app_banner_manager_android.h"
#include "chrome/browser/android/bookmarks/bookmark_bridge.h"
#include "chrome/browser/android/bookmarks/partner_bookmarks_reader.h"
#include "chrome/browser/android/bottombar/overlay_panel_content.h"
#include "chrome/browser/android/browsing_data/browsing_data_bridge.h"
#include "chrome/browser/android/browsing_data/browsing_data_counter_bridge.h"
#include "chrome/browser/android/browsing_data/url_filter_bridge.h"
#include "chrome/browser/android/chrome_application.h"
#include "chrome/browser/android/chrome_backup_agent.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/contextualsearch/contextual_search_context.h"
#include "chrome/browser/android/contextualsearch/contextual_search_manager.h"
#include "chrome/browser/android/contextualsearch/contextual_search_ranker_logger_impl.h"
#include "chrome/browser/android/contextualsearch/contextual_search_tab_helper.h"
#include "chrome/browser/android/contextualsearch/ctr_suppression.h"
#include "chrome/browser/android/cookies/cookies_fetcher.h"
#include "chrome/browser/android/customtabs/origin_verifier.h"
#include "chrome/browser/android/data_usage/data_use_tab_ui_manager_android.h"
#include "chrome/browser/android/data_usage/external_data_use_observer_bridge.h"
#include "chrome/browser/android/devtools_server.h"
#include "chrome/browser/android/document/document_web_contents_delegate.h"
#include "chrome/browser/android/dom_distiller/distiller_ui_handle_android.h"
#include "chrome/browser/android/download/download_controller.h"
#include "chrome/browser/android/download/download_manager_service.h"
#include "chrome/browser/android/download/service/download_background_task.h"
#include "chrome/browser/android/download/ui/thumbnail_provider.h"
#include "chrome/browser/android/favicon_helper.h"
#include "chrome/browser/android/feature_engagement_tracker/feature_engagement_tracker_factory_android.h"
#include "chrome/browser/android/feature_utilities.h"
#include "chrome/browser/android/feedback/connectivity_checker.h"
#include "chrome/browser/android/feedback/screenshot_task.h"
#include "chrome/browser/android/find_in_page/find_in_page_bridge.h"
#include "chrome/browser/android/foreign_session_helper.h"
#include "chrome/browser/android/history/browsing_history_bridge.h"
#include "chrome/browser/android/history_report/history_report_jni_bridge.h"
#include "chrome/browser/android/instantapps/instant_apps_infobar_delegate.h"
#include "chrome/browser/android/instantapps/instant_apps_settings.h"
#include "chrome/browser/android/large_icon_bridge.h"
#include "chrome/browser/android/locale/locale_manager.h"
#include "chrome/browser/android/locale/special_locale_handler.h"
#include "chrome/browser/android/location_settings_impl.h"
#include "chrome/browser/android/logo_bridge.h"
#include "chrome/browser/android/metrics/launch_metrics.h"
#include "chrome/browser/android/metrics/uma_session_stats.h"
#include "chrome/browser/android/metrics/uma_utils.h"
#include "chrome/browser/android/metrics/variations_session.h"
#include "chrome/browser/android/net/external_estimate_provider_android.h"
#include "chrome/browser/android/ntp/content_suggestions_notification_helper.h"
#include "chrome/browser/android/ntp/most_visited_sites_bridge.h"
#include "chrome/browser/android/ntp/ntp_snippets_bridge.h"
#include "chrome/browser/android/ntp/recent_tabs_page_prefs.h"
#include "chrome/browser/android/ntp/suggestions_event_reporter_bridge.h"
#include "chrome/browser/android/offline_pages/background_scheduler_bridge.h"
#include "chrome/browser/android/offline_pages/downloads/offline_page_download_bridge.h"
#include "chrome/browser/android/offline_pages/offline_page_bridge.h"
#include "chrome/browser/android/offline_pages/prefetch/prefetch_background_task.h"
#include "chrome/browser/android/omnibox/answers_image_bridge.h"
#include "chrome/browser/android/omnibox/autocomplete_controller_android.h"
#include "chrome/browser/android/omnibox/omnibox_prerender.h"
#include "chrome/browser/android/password_ui_view_android.h"
#include "chrome/browser/android/payments/service_worker_payment_app_bridge.h"
#include "chrome/browser/android/physical_web/eddystone_encoder_bridge.h"
#include "chrome/browser/android/physical_web/physical_web_data_source_android.h"
#include "chrome/browser/android/policy/policy_auditor.h"
#include "chrome/browser/android/preferences/autofill/autofill_profile_bridge.h"
#include "chrome/browser/android/preferences/pref_service_bridge.h"
#include "chrome/browser/android/preferences/website_preference_bridge.h"
#include "chrome/browser/android/profiles/profile_downloader_android.h"
#include "chrome/browser/android/provider/chrome_browser_provider.h"
#include "chrome/browser/android/rappor/rappor_service_bridge.h"
#include "chrome/browser/android/recently_closed_tabs_bridge.h"
#include "chrome/browser/android/rlz/revenue_stats.h"
#include "chrome/browser/android/rlz/rlz_ping_handler.h"
#include "chrome/browser/android/search_geolocation/search_geolocation_disclosure_tab_helper.h"
#include "chrome/browser/android/service_tab_launcher.h"
#include "chrome/browser/android/sessions/session_tab_helper_android.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/signin/account_management_screen_helper.h"
#include "chrome/browser/android/signin/account_tracker_service_android.h"
#include "chrome/browser/android/signin/signin_investigator_android.h"
#include "chrome/browser/android/signin/signin_manager_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_state.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/android/url_utilities.h"
#include "chrome/browser/android/voice_search_tab_helper.h"
#include "chrome/browser/android/warmup_manager.h"
#include "chrome/browser/android/web_contents_factory.h"
#include "chrome/browser/android/webapk/chrome_webapk_host.h"
#include "chrome/browser/android/webapk/webapk_installer.h"
#include "chrome/browser/android/webapk/webapk_update_data_fetcher.h"
#include "chrome/browser/android/webapk/webapk_update_manager.h"
#include "chrome/browser/android/webapps/add_to_homescreen_manager.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/android/phone_number_util_android.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory_android.h"
#include "chrome/browser/dom_distiller/tab_utils_android.h"
#include "chrome/browser/engagement/site_engagement_service_android.h"
#include "chrome/browser/history/android/sqlite_cursor.h"
#include "chrome/browser/invalidation/invalidation_service_factory_android.h"
#include "chrome/browser/media/android/cdm/media_drm_credential_manager.h"
#include "chrome/browser/media/android/remote/record_cast_action.h"
#include "chrome/browser/media/android/remote/remote_media_player_bridge.h"
#include "chrome/browser/media/android/router/media_router_android.h"
#include "chrome/browser/media/android/router/media_router_dialog_controller_android.h"
#include "chrome/browser/net/spdyproxy/data_reduction_promo_infobar_delegate_android.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"
#include "chrome/browser/notifications/notification_platform_bridge_android.h"
#include "chrome/browser/password_manager/account_chooser_dialog_android.h"
#include "chrome/browser/password_manager/auto_signin_first_run_dialog_android.h"
#include "chrome/browser/payments/android/chrome_payments_jni_registrar.h"
#include "chrome/browser/permissions/permission_dialog_delegate.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/browser/predictors/loading_predictor_android.h"
#include "chrome/browser/prerender/external_prerender_handler_android.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search_engines/template_url_service_android.h"
#include "chrome/browser/signin/oauth2_token_service_delegate_android.h"
#include "chrome/browser/speech/tts_android.h"
#include "chrome/browser/ssl/security_state_model_android.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_android.h"
#include "chrome/browser/supervised_user/supervised_user_content_provider_android.h"
#include "chrome/browser/sync/profile_sync_service_android.h"
#include "chrome/browser/sync/sessions/sync_sessions_metrics_android.h"
#include "components/feature_engagement_tracker/public/android/feature_engagement_tracker_jni_registrar.h"
#include "components/offline_pages/features/features.h"
#include "components/safe_browsing_db/android/jni_registrar.h"
#include "components/spellcheck/browser/android/component_jni_registrar.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "components/sync/android/sync_jni_registrar.h"
#include "components/variations/android/component_jni_registrar.h"
#include "printing/features/features.h"

#if BUILDFLAG(ENABLE_PRINTING) && !BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "printing/printing_context_android.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES_HARNESS)
#include "chrome/browser/android/offline_pages/evaluation/offline_page_evaluation_bridge.h"
#endif

namespace android {

static base::android::RegistrationMethod kChromeRegisteredMethods[] = {
    // Register JNI for chrome classes.
    {"AccountChooserDialogAndroid", RegisterAccountChooserDialogAndroid},
    {"AutoSigninFirstRunDialogAndroid",
     RegisterAutoSigninFirstRunDialogAndroid},
    {"AccountManagementScreenHelper", AccountManagementScreenHelper::Register},
    {"AccountTrackerService", signin::android::RegisterAccountTrackerService},
    {"AddToHomescreenManager", AddToHomescreenManager::Register},
    {"AfterStartupTaskUtils", RegisterAfterStartupTaskUtilsJNI},
    {"AnswersImageBridge", RegisterAnswersImageBridge},
    {"AppBannerInfoBarDelegateAndroid",
     banners::RegisterAppBannerInfoBarDelegateAndroid},
    {"AppBannerManagerAndroid", banners::AppBannerManagerAndroid::Register},
    {"AutocompleteControllerAndroid", RegisterAutocompleteControllerAndroid},
    {"AutofillProfileBridge", autofill::RegisterAutofillProfileBridge},
    {"BackgroundSchedulerBridge",
     offline_pages::android::RegisterBackgroundSchedulerBridge},
    {"PrefetchBackgroundTask", offline_pages::RegisterPrefetchBackgroundTask},
    {"BookmarkBridge", BookmarkBridge::RegisterBookmarkBridge},
    {"BrowsingDataBridge", RegisterBrowsingDataBridge},
    {"BrowsingDataCounterBridge", BrowsingDataCounterBridge::Register},
    {"BrowsingHistoryBridge", RegisterBrowsingHistoryBridge},
    {"ChildAccountService", RegisterChildAccountService},
    {"ChromeApplication", chrome::android::ChromeApplication::RegisterBindings},
    {"ChromeBackupAgent", chrome::android::RegisterBackupAgent},
    {"ChromeBrowserProvider",
     ChromeBrowserProvider::RegisterChromeBrowserProvider},
    {"ChromeFeatureList", chrome::android::RegisterChromeFeatureListJni},
    {"ChromeMediaRouter", media_router::MediaRouterAndroidBridge::Register},
    {"ChromeMediaRouterDialogController",
     media_router::MediaRouterDialogControllerAndroid::Register},
    {"ChromePayments", payments::android::RegisterChromePayments},
    {"ChromeWebApkHost", ChromeWebApkHost::Register},
    {"SecurityStateModel", RegisterSecurityStateModelAndroid},
    {"ConnectivityChecker", chrome::android::RegisterConnectivityChecker},
    {"ContentSuggestionsNotificationHelper",
     ntp_snippets::ContentSuggestionsNotificationHelper::Register},
    {"ContextualSearchContext", RegisterContextualSearchContext},
    {"ContextualSearchManager", RegisterContextualSearchManager},
    {"ContextualSearchRankerLoggerImpl",
     RegisterContextualSearchRankerLoggerImpl},
    {"ContextualSearchTabHelper", RegisterContextualSearchTabHelper},
    {"CookiesFetcher", RegisterCookiesFetcher},
    {"CtrSuppression", RegisterCtrSuppression},
    {"DataReductionPromoInfoBarDelegate",
     DataReductionPromoInfoBarDelegateAndroid::Register},
    {"DataReductionProxySettings", DataReductionProxySettingsAndroid::Register},
    {"DataUseTabUIManager", RegisterDataUseTabUIManager},
    {"DevToolsServer", RegisterDevToolsServer},
    {"DocumentWebContentsDelegate", DocumentWebContentsDelegate::Register},
    {"DomDistillerServiceFactory",
     dom_distiller::android::DomDistillerServiceFactoryAndroid::Register},
    {"DomDistillerTabUtils", RegisterDomDistillerTabUtils},
    {"DownloadBackgroundTask",
     download::android::RegisterDownloadBackgroundTask},
    {"DownloadController", DownloadController::RegisterDownloadController},
    {"DownloadManagerService",
     DownloadManagerService::RegisterDownloadManagerService},
    {"EddystoneEncoderBridge", RegisterEddystoneEncoderBridge},
    {"ExternalDataUseObserverBridge",
     chrome::android::RegisterExternalDataUseObserver},
    {"ExternalPrerenderRequestHandler",
     prerender::ExternalPrerenderHandlerAndroid::
         RegisterExternalPrerenderHandlerAndroid},
    {"FaviconHelper", FaviconHelper::RegisterFaviconHelper},
    {"FeatureEngagementTracker",
     feature_engagement_tracker::RegisterFeatureEngagementTrackerJni},
    {"FeatureEngagementTrackerFactory",
     RegisterFeatureEngagementTrackerFactoryJni},
    {"FeatureUtilities", RegisterFeatureUtilities},
    {"FindInPageBridge", FindInPageBridge::RegisterFindInPageBridge},
    {"FontSizePrefsAndroid", FontSizePrefsAndroid::Register},
    {"ForeignSessionHelper",
     ForeignSessionHelper::RegisterForeignSessionHelper},
    {"HistoryReportJniBridge", history_report::RegisterHistoryReportJniBridge},
    {"InstantAppsInfobarDelegate", RegisterInstantAppsInfoBarDelegate},
    {"InstantAppsSettings", RegisterInstantAppsSettings},
    {"InvalidationServiceFactory",
     invalidation::InvalidationServiceFactoryAndroid::Register},
    {"LargeIconBridge", LargeIconBridge::RegisterLargeIconBridge},
    {"LaunchMetrics", metrics::RegisterLaunchMetrics},
    {"LoadingPredictor", predictors::RegisterLoadingPredictor},
    {"LocaleManager", RegisterLocaleManager},
    {"LocationSettingsImpl", LocationSettingsImpl::Register},
    {"LogoBridge", RegisterLogoBridge},
    {"MediaDrmCredentialManager",
     MediaDrmCredentialManager::RegisterMediaDrmCredentialManager},
    {"MostVisitedSitesBridge", MostVisitedSitesBridge::Register},
    {"ExternalEstimateProviderAndroid",
     chrome::android::RegisterExternalEstimateProviderAndroid},
    {"RecentTabsPagePrefs", RecentTabsPagePrefs::RegisterJni},
    {"NotificationPlatformBridge",
     NotificationPlatformBridgeAndroid::RegisterNotificationPlatformBridge},
    {"NTPSnippetsBridge", NTPSnippetsBridge::Register},
    {"OAuth2TokenServiceDelegateAndroid",
     OAuth2TokenServiceDelegateAndroid::Register},
    {"OfflinePageBridge", offline_pages::android::RegisterOfflinePageBridge},
    {"OfflinePageDownloadBridge",
     offline_pages::android::OfflinePageDownloadBridge::Register},
#if BUILDFLAG(ENABLE_OFFLINE_PAGES_HARNESS)
    {"OfflinePageEvaluationBridge",
     offline_pages::android::OfflinePageEvaluationBridge::Register},
#endif
    {"OmniboxPrerender", RegisterOmniboxPrerender},
    {"OriginVerifier", customtabs::RegisterOriginVerifier},
    {"OverlayPanelContent", RegisterOverlayPanelContent},
    {"PartnerBookmarksReader",
     PartnerBookmarksReader::RegisterPartnerBookmarksReader},
    {"PasswordUIViewAndroid",
     PasswordUIViewAndroid::RegisterPasswordUIViewAndroid},
    {"PermissionDialogDelegate",
     PermissionDialogDelegate::RegisterPermissionDialogDelegate},
    {"PermissionUpdateInfoBarDelegate",
     PermissionUpdateInfoBarDelegate::RegisterPermissionUpdateInfoBarDelegate},
    {"PersonalDataManagerAndroid",
     autofill::PersonalDataManagerAndroid::Register},
    {"PhoneNumberUtil", RegisterPhoneNumberUtil},
    {"PhysicalWebDataSourceAndroid",
     PhysicalWebDataSourceAndroid::RegisterPhysicalWebDataSource},
    {"PolicyAuditor", RegisterPolicyAuditor},
    {"PrefServiceBridge", PrefServiceBridge::RegisterPrefServiceBridge},
    {"ProfileAndroid", ProfileAndroid::RegisterProfileAndroid},
    {"ProfileDownloader", RegisterProfileDownloader},
    {"ProfileSyncService", ProfileSyncServiceAndroid::Register},
    {"RapporServiceBridge", rappor::RegisterRapporServiceBridge},
    {"RecentlyClosedBridge", RecentlyClosedTabsBridge::Register},
    {"RecordCastAction", remote_media::RegisterRecordCastAction},
    {"RemoteMediaPlayerBridge",
     remote_media::RemoteMediaPlayerBridge::RegisterRemoteMediaPlayerBridge},
    {"RevenueStats", chrome::android::RegisterRevenueStats},
    {"RlzPingHandler", chrome::android::RegisterRlzPingHandler},
    {"SafeBrowsing", safe_browsing::android::RegisterBrowserJNI},
    {"ScreenshotTask", chrome::android::RegisterScreenshotTask},
    {"ServiceTabLauncher", ServiceTabLauncher::Register},
    {"SearchGeolocationDisclosureTabHelper",
     SearchGeolocationDisclosureTabHelper::Register},
    {"ServiceWorkerPaymentAppBridge", RegisterServiceWorkerPaymentAppBridge},
    {"SessionTabHelper", RegisterSessionTabHelper},
    {"ShortcutHelper", ShortcutHelper::RegisterShortcutHelper},
    {"SigninInvestigator", SigninInvestigatorAndroid::Register},
    {"SigninManager", SigninManagerAndroid::Register},
    {"SiteEngagementService", SiteEngagementServiceAndroid::Register},
    {"SpecialLocaleHandler", RegisterSpecialLocaleHandler},
#if BUILDFLAG(ENABLE_SPELLCHECK)
    {"SpellCheckerSessionBridge", spellcheck::android::RegisterSpellcheckJni},
#endif
    {"SqliteCursor", SQLiteCursor::RegisterSqliteCursor},
    {"StartupMetricUtils", chrome::android::RegisterStartupMetricUtils},
    {"SuggestionsEventReporterBridge", RegisterSuggestionsEventReporterBridge},
    {"SupervisedUserContentProvider", SupervisedUserContentProvider::Register},
    {"Sync", syncer::RegisterSyncJni},
    {"SyncSessionsMetrics", SyncSessionsMetricsAndroid::Register},
    {"TabAndroid", TabAndroid::RegisterTabAndroid},
    {"TabState", RegisterTabState},
    {"TabWebContentsDelegateAndroid", RegisterTabWebContentsDelegateAndroid},
    {"TemplateUrlServiceAndroid", TemplateUrlServiceAndroid::Register},
    {"ThumbnailProvider", ThumbnailProvider::RegisterThumbnailProvider},
    {"TtsPlatformImpl", TtsPlatformImplAndroid::Register},
    {"UmaSessionStats", RegisterUmaSessionStats},
    {"UrlFilterBridge", UrlFilterBridge::Register},
    {"UrlUtilities", RegisterUrlUtilities},
    {"Variations", variations::android::RegisterVariations},
    {"VariationsSession", chrome::android::RegisterVariationsSession},
    {"WarmupManager", RegisterWarmupManager},
    {"WebApkInstaller", WebApkInstaller::Register},
    {"WebApkUpdateManager", WebApkUpdateManager::Register},
    {"WebApkUpdateDataFetcher", WebApkUpdateDataFetcher::Register},
    {"WebContentsFactory", RegisterWebContentsFactory},
    {"WebsitePreferenceBridge", RegisterWebsitePreferenceBridge},
#if BUILDFLAG(ENABLE_PRINTING) && !BUILDFLAG(ENABLE_PRINT_PREVIEW)
    {"PrintingContext",
     printing::PrintingContextAndroid::RegisterPrintingContext},
#endif
};

bool RegisterBrowserJNI(JNIEnv* env) {
  TRACE_EVENT0("startup", "chrome_android::RegisterJni");
  return RegisterNativeMethods(env, kChromeRegisteredMethods,
                               arraysize(kChromeRegisteredMethods));
}

}  // namespace android
