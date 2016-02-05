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
#include "chrome/browser/android/accessibility_util.h"
#include "chrome/browser/android/appmenu/app_menu_drag_helper.h"
#include "chrome/browser/android/background_sync_launcher_android.h"
#include "chrome/browser/android/banners/app_banner_infobar_delegate_android.h"
#include "chrome/browser/android/banners/app_banner_manager_android.h"
#include "chrome/browser/android/bookmarks/bookmarks_bridge.h"
#include "chrome/browser/android/bookmarks/partner_bookmarks_reader.h"
#include "chrome/browser/android/bottombar/overlay_panel_content.h"
#include "chrome/browser/android/browsing_data/browsing_data_counter_bridge.h"
#include "chrome/browser/android/chrome_application.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/compositor/compositor_view.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "chrome/browser/android/compositor/scene_layer/contextual_search_scene_layer.h"
#include "chrome/browser/android/compositor/scene_layer/reader_mode_scene_layer.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"
#include "chrome/browser/android/compositor/scene_layer/static_tab_scene_layer.h"
#include "chrome/browser/android/compositor/scene_layer/tab_list_scene_layer.h"
#include "chrome/browser/android/compositor/scene_layer/tab_strip_scene_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "chrome/browser/android/contextualsearch/contextual_search_manager.h"
#include "chrome/browser/android/contextualsearch/contextual_search_tab_helper.h"
#include "chrome/browser/android/cookies/cookies_fetcher.h"
#include "chrome/browser/android/data_usage/data_use_tab_ui_manager_android.h"
#include "chrome/browser/android/data_usage/external_data_use_observer_bridge.h"
#include "chrome/browser/android/dev_tools_server.h"
#include "chrome/browser/android/document/document_web_contents_delegate.h"
#include "chrome/browser/android/dom_distiller/distiller_ui_handle_android.h"
#include "chrome/browser/android/download/chrome_download_delegate.h"
#include "chrome/browser/android/download/download_manager_service.h"
#include "chrome/browser/android/favicon_helper.h"
#include "chrome/browser/android/feature_utilities.h"
#include "chrome/browser/android/feedback/connectivity_checker.h"
#include "chrome/browser/android/feedback/screenshot_task.h"
#include "chrome/browser/android/find_in_page/find_in_page_bridge.h"
#include "chrome/browser/android/foreign_session_helper.h"
#include "chrome/browser/android/fullscreen/fullscreen_infobar_delegate.h"
#include "chrome/browser/android/history_report/history_report_jni_bridge.h"
#include "chrome/browser/android/intent_helper.h"
#include "chrome/browser/android/java_exception_reporter.h"
#include "chrome/browser/android/large_icon_bridge.h"
#include "chrome/browser/android/location_settings_impl.h"
#include "chrome/browser/android/logo_bridge.h"
#include "chrome/browser/android/metrics/launch_metrics.h"
#include "chrome/browser/android/metrics/uma_session_stats.h"
#include "chrome/browser/android/metrics/uma_utils.h"
#include "chrome/browser/android/metrics/variations_session.h"
#include "chrome/browser/android/most_visited_sites.h"
#include "chrome/browser/android/net/external_estimate_provider_android.h"
#include "chrome/browser/android/new_tab_page_prefs.h"
#include "chrome/browser/android/offline_pages/offline_page_bridge.h"
#include "chrome/browser/android/omnibox/answers_image_bridge.h"
#include "chrome/browser/android/omnibox/autocomplete_controller_android.h"
#include "chrome/browser/android/omnibox/omnibox_prerender.h"
#include "chrome/browser/android/password_ui_view_android.h"
#include "chrome/browser/android/precache/precache_launcher.h"
#include "chrome/browser/android/preferences/autofill/autofill_profile_bridge.h"
#include "chrome/browser/android/preferences/pref_service_bridge.h"
#include "chrome/browser/android/preferences/website_preference_bridge.h"
#include "chrome/browser/android/profiles/profile_downloader_android.h"
#include "chrome/browser/android/provider/chrome_browser_provider.h"
#include "chrome/browser/android/rappor/rappor_service_bridge.h"
#include "chrome/browser/android/recently_closed_tabs_bridge.h"
#include "chrome/browser/android/rlz/revenue_stats.h"
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
#include "chrome/browser/android/webapps/add_to_homescreen_dialog_helper.h"
#include "chrome/browser/android/webapps/webapp_registry.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory_android.h"
#include "chrome/browser/dom_distiller/tab_utils_android.h"
#include "chrome/browser/history/android/sqlite_cursor.h"
#include "chrome/browser/interests/android/interests_service.h"
#include "chrome/browser/invalidation/invalidation_service_factory_android.h"
#include "chrome/browser/lifetime/application_lifetime_android.h"
#include "chrome/browser/media/android/cdm/media_drm_credential_manager.h"
#include "chrome/browser/media/android/remote/record_cast_action.h"
#include "chrome/browser/media/android/remote/remote_media_player_bridge.h"
#include "chrome/browser/media/android/router/media_router_android.h"
#include "chrome/browser/media/android/router/media_router_dialog_controller_android.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"
#include "chrome/browser/notifications/notification_ui_manager_android.h"
#include "chrome/browser/password_manager/account_chooser_dialog_android.h"
#include "chrome/browser/password_manager/credential_android.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prerender/external_prerender_handler_android.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/push_messaging/push_messaging_service_observer_android.h"
#include "chrome/browser/search_engines/template_url_service_android.h"
#include "chrome/browser/signin/oauth2_token_service_delegate_android.h"
#include "chrome/browser/speech/tts_android.h"
#include "chrome/browser/spellchecker/spellchecker_session_bridge_android.h"
#include "chrome/browser/ssl/security_state_model_android.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_feedback_reporter_android.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_android.h"
#include "chrome/browser/supervised_user/supervised_user_content_provider_android.h"
#include "chrome/browser/sync/profile_sync_service_android.h"
#include "chrome/browser/ui/android/autofill/autofill_dialog_controller_android.h"
#include "chrome/browser/ui/android/autofill/autofill_dialog_result.h"
#include "chrome/browser/ui/android/autofill/autofill_keyboard_accessory_view.h"
#include "chrome/browser/ui/android/autofill/autofill_logger_android.h"
#include "chrome/browser/ui/android/autofill/autofill_popup_view_android.h"
#include "chrome/browser/ui/android/autofill/card_unmask_prompt_view_android.h"
#include "chrome/browser/ui/android/autofill/credit_card_scanner_view_android.h"
#include "chrome/browser/ui/android/autofill/password_generation_popup_view_android.h"
#include "chrome/browser/ui/android/bluetooth_chooser_android.h"
#include "chrome/browser/ui/android/certificate_viewer_android.h"
#include "chrome/browser/ui/android/chrome_http_auth_handler.h"
#include "chrome/browser/ui/android/connection_info_popup_android.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/android/infobars/app_banner_infobar_android.h"
#include "chrome/browser/ui/android/infobars/auto_signin_first_run_infobar.h"
#include "chrome/browser/ui/android/infobars/autofill_save_card_infobar.h"
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"
#include "chrome/browser/ui/android/infobars/data_reduction_proxy_infobar.h"
#include "chrome/browser/ui/android/infobars/download_overwrite_infobar.h"
#include "chrome/browser/ui/android/infobars/generated_password_saved_infobar.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"
#include "chrome/browser/ui/android/infobars/infobar_container_android.h"
#include "chrome/browser/ui/android/infobars/save_password_infobar.h"
#include "chrome/browser/ui/android/infobars/simple_confirm_infobar_builder.h"
#include "chrome/browser/ui/android/infobars/translate_infobar.h"
#include "chrome/browser/ui/android/javascript_app_modal_dialog_android.h"
#include "chrome/browser/ui/android/omnibox/omnibox_url_emphasizer.h"
#include "chrome/browser/ui/android/omnibox/omnibox_view_util.h"
#include "chrome/browser/ui/android/snackbars/auto_signin_prompt_controller.h"
#include "chrome/browser/ui/android/ssl_client_certificate_request.h"
#include "chrome/browser/ui/android/tab_model/single_tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"
#include "chrome/browser/ui/android/toolbar/toolbar_model_android.h"
#include "chrome/browser/ui/android/website_settings_popup_android.h"
#include "components/bookmarks/common/android/component_jni_registrar.h"
#include "components/dom_distiller/android/component_jni_registrar.h"
#include "components/gcm_driver/android/component_jni_registrar.h"
#include "components/invalidation/impl/android/component_jni_registrar.h"
#include "components/navigation_interception/component_jni_registrar.h"
#include "components/policy/core/browser/android/component_jni_registrar.h"
#include "components/safe_json/android/component_jni_registrar.h"
#include "components/service_tab_launcher/component_jni_registrar.h"
#include "components/signin/core/browser/android/component_jni_registrar.h"
#include "components/variations/android/component_jni_registrar.h"
#include "components/variations/android/variations_seed_bridge.h"
#include "components/web_contents_delegate_android/component_jni_registrar.h"
#include "sync/android/sync_jni_registrar.h"

#if defined(ENABLE_PRINTING) && !defined(ENABLE_PRINT_PREVIEW)
#include "printing/printing_context_android.h"
#endif

namespace chrome {
namespace android {

static base::android::RegistrationMethod kChromeRegisteredMethods[] = {
    // Register JNI for components we depend on.
    {"AppMenuDragHelper", RegisterAppMenuDragHelper},
    {"Bookmarks", bookmarks::android::RegisterBookmarks},
    {"DomDistiller", dom_distiller::android::RegisterDomDistiller},
    {"ChromeDownloadDelegate", RegisterChromeDownloadDelegate},
    {"GCMDriver", gcm::android::RegisterGCMDriverJni},
    {"Invalidation", invalidation::android::RegisterInvalidationJni},
    {"NavigationInterception",
     navigation_interception::RegisterNavigationInterceptionJni},
    {"Policy", policy::android::RegisterPolicy},
    {"SafeJson", safe_json::android::RegisterSafeJsonJni},
    {"Signin", signin::android::RegisterSigninJni},
    {"WebContentsDelegateAndroid",
     web_contents_delegate_android::RegisterWebContentsDelegateAndroidJni},
    // Register JNI for chrome classes.
    {"AccessibilityUtils", AccessibilityUtil::Register},
    {"AccountChooserDialogAndroid", RegisterAccountChooserDialogAndroid},
    {"AccountManagementScreenHelper", AccountManagementScreenHelper::Register},
    {"AccountTrackerService", signin::android::RegisterAccountTrackerService},
    {"AddToHomescreenDialogHelper",
     AddToHomescreenDialogHelper::RegisterAddToHomescreenDialogHelper},
    {"AfterStartupTaskUtils", RegisterAfterStartupTaskUtilsJNI},
    {"AnswersImageBridge", RegisterAnswersImageBridge},
    {"AppBannerInfoBarAndroid", RegisterAppBannerInfoBarAndroid},
    {"AppBannerInfoBarDelegateAndroid",
     banners::RegisterAppBannerInfoBarDelegateAndroid},
    {"AppBannerManagerAndroid", banners::AppBannerManagerAndroid::Register},
    {"ApplicationLifetime", RegisterApplicationLifetimeAndroid},
    {"AutocompleteControllerAndroid", RegisterAutocompleteControllerAndroid},
    {"AutofillSaveCardInfoBar", AutofillSaveCardInfoBar::Register},
    {"AutofillDialogControllerAndroid",
     autofill::AutofillDialogControllerAndroid::
         RegisterAutofillDialogControllerAndroid},
    {"AutofillDialogResult",
     autofill::AutofillDialogResult::RegisterAutofillDialogResult},
    {"AutofillKeyboardAccessory", autofill::AutofillKeyboardAccessoryView::
                                      RegisterAutofillKeyboardAccessoryView},
    {"AutofillLoggerAndroid", autofill::AutofillLoggerAndroid::Register},
    {"AutofillPopup",
     autofill::AutofillPopupViewAndroid::RegisterAutofillPopupViewAndroid},
    {"AutofillProfileBridge", autofill::RegisterAutofillProfileBridge},
    {"AutoSigninSnackbarController", RegisterAutoSigninSnackbarController},
    {"BackgroundSyncLauncherAndroid",
     BackgroundSyncLauncherAndroid::RegisterLauncher},
    {"BluetoothChooserAndroid", BluetoothChooserAndroid::Register},
    {"BookmarksBridge", BookmarksBridge::RegisterBookmarksBridge},
    {"BrowsingDataCounterBridge", BrowsingDataCounterBridge::Register},
    {"CardUnmaskPrompt", autofill::CardUnmaskPromptViewAndroid::Register},
    {"CertificateViewer", RegisterCertificateViewer},
    {"ChildAccountFeedbackReporter", RegisterChildAccountFeedbackReporter},
    {"ChildAccountService", RegisterChildAccountService},
    {"ChromeApplication", ChromeApplication::RegisterBindings},
    {"ChromeBrowserProvider",
     ChromeBrowserProvider::RegisterChromeBrowserProvider},
    {"ChromeFeatureList", RegisterChromeFeatureListJni},
    {"ChromeHttpAuthHandler",
     ChromeHttpAuthHandler::RegisterChromeHttpAuthHandler},
#if defined(ENABLE_MEDIA_ROUTER)
    {"ChromeMediaRouter", media_router::MediaRouterAndroid::Register},
    {"ChromeMediaRouterDialogController",
     media_router::MediaRouterDialogControllerAndroid::Register},
#endif
    {"CompositorView", RegisterCompositorView},
    {"ConfirmInfoBar", RegisterConfirmInfoBar},
    {"ConnectionInfoPopupAndroid",
     ConnectionInfoPopupAndroid::RegisterConnectionInfoPopupAndroid},
    {"SecurityStateModel", RegisterSecurityStateModelAndroid},
    {"ConnectivityChecker", RegisterConnectivityChecker},
    {"ContextMenuHelper", RegisterContextMenuHelper},
    {"ContextualSearchManager", RegisterContextualSearchManager},
    {"ContextualSearchSceneLayer", RegisterContextualSearchSceneLayer},
    {"ContextualSearchTabHelper", RegisterContextualSearchTabHelper},
    {"CookiesFetcher", RegisterCookiesFetcher},
    {"Credential", RegisterCredential},
    {"CreditCardScanner", autofill::CreditCardScannerViewAndroid::Register},
    {"DataReductionProxyInfoBarDelegate", DataReductionProxyInfoBar::Register},
    {"DataReductionProxySettings", DataReductionProxySettingsAndroid::Register},
    {"DataUseTabUIManager", RegisterDataUseTabUIManager},
    {"DevToolsServer", RegisterDevToolsServer},
    {"DocumentWebContentsDelegate", DocumentWebContentsDelegate::Register},
    {"DomDistillerServiceFactory",
     dom_distiller::android::DomDistillerServiceFactoryAndroid::Register},
    {"DomDistillerTabUtils", RegisterDomDistillerTabUtils},
    {"DownloadManagerService",
     DownloadManagerService::RegisterDownloadManagerService},
    {"DownloadOverwriteInfoBarDelegate",
     RegisterDownloadOverwriteInfoBarDelegate},
    {"ExternalDataUseObserverBridge", RegisterExternalDataUseObserver},
    {"ExternalPrerenderRequestHandler",
     prerender::ExternalPrerenderHandlerAndroid::
         RegisterExternalPrerenderHandlerAndroid},
    {"FaviconHelper", FaviconHelper::RegisterFaviconHelper},
    {"FeatureUtilities", RegisterFeatureUtilities},
    {"DomDistillerUIUtils", dom_distiller::android::RegisterUIHandle},
    {"FindInPageBridge", FindInPageBridge::RegisterFindInPageBridge},
    {"FontSizePrefsAndroid", FontSizePrefsAndroid::Register},
    {"ForeignSessionHelper",
     ForeignSessionHelper::RegisterForeignSessionHelper},
    {"FullscreenInfoBarDelegate",
     FullscreenInfoBarDelegate::RegisterFullscreenInfoBarDelegate},
    {"GeneratedPasswordSavedInfoBarDelegate",
     RegisterGeneratedPasswordSavedInfoBarDelegate},
    {"HistoryReportJniBridge", history_report::RegisterHistoryReportJniBridge},
    {"InfoBarContainer", RegisterInfoBarContainer},
    {"InterestsService", InterestsService::Register},
    {"InvalidationServiceFactory",
     invalidation::InvalidationServiceFactoryAndroid::Register},
    {"SimpleConfirmInfoBarBuilder", RegisterSimpleConfirmInfoBarBuilder},
    {"ShortcutHelper", ShortcutHelper::RegisterShortcutHelper},
    {"IntentHelper", RegisterIntentHelper},
    {"JavaExceptionReporter", RegisterJavaExceptionReporterJni},
    {"JavascriptAppModalDialog",
     JavascriptAppModalDialogAndroid::RegisterJavascriptAppModalDialog},
    {"LargeIconBridge", LargeIconBridge::RegisterLargeIconBridge},
    {"LaunchMetrics", metrics::RegisterLaunchMetrics},
    {"LayerTitleCache", chrome::android::RegisterLayerTitleCache},
    {"LocationSettings", LocationSettingsImpl::Register},
    {"LogoBridge", RegisterLogoBridge},
    {"MediaDrmCredentialManager",
     MediaDrmCredentialManager::RegisterMediaDrmCredentialManager},
    {"MostVisitedSites", MostVisitedSites::Register},
    {"NativeInfoBar", RegisterNativeInfoBar},
    {"ExternalEstimateProviderAndroid",
     RegisterExternalEstimateProviderAndroid},
    {"NewTabPagePrefs", NewTabPagePrefs::RegisterNewTabPagePrefs},
    {"NotificationUIManager",
     NotificationUIManagerAndroid::RegisterNotificationUIManager},
    {"OAuth2TokenServiceDelegateAndroid",
     OAuth2TokenServiceDelegateAndroid::Register},
    {"OfflinePageBridge", offline_pages::android::RegisterOfflinePageBridge},
    {"OmniboxPrerender", RegisterOmniboxPrerender},
    {"OmniboxUrlEmphasizer",
     OmniboxUrlEmphasizer::RegisterOmniboxUrlEmphasizer},
    {"OmniboxViewUtil", OmniboxViewUtil::RegisterOmniboxViewUtil},
    {"OverlayPanelContent", RegisterOverlayPanelContent},
    {"PlatformUtil", platform_util::RegisterPlatformUtil},
    {"PartnerBookmarksReader",
     PartnerBookmarksReader::RegisterPartnerBookmarksReader},
    {"PasswordGenerationPopup",
     autofill::PasswordGenerationPopupViewAndroid::Register},
    {"PasswordUIViewAndroid",
     PasswordUIViewAndroid::RegisterPasswordUIViewAndroid},
    {"PermissionUpdateInfoBarDelegate",
     PermissionUpdateInfoBarDelegate::RegisterPermissionUpdateInfoBarDelegate},
    {"PersonalDataManagerAndroid",
     autofill::PersonalDataManagerAndroid::Register},
    {"PrecacheLauncher", RegisterPrecacheLauncher},
    {"PrefServiceBridge", PrefServiceBridge::RegisterPrefServiceBridge},
    {"ProfileAndroid", ProfileAndroid::RegisterProfileAndroid},
    {"ProfileDownloader", RegisterProfileDownloader},
    {"ProfileSyncService", ProfileSyncServiceAndroid::Register},
    {"PushMessagingServiceObserver",
     PushMessagingServiceObserverAndroid::RegisterJni},
    {"RapporServiceBridge", rappor::RegisterRapporServiceBridge},
    {"RecentlyClosedBridge", RecentlyClosedTabsBridge::Register},
    {"RecordCastAction", remote_media::RegisterRecordCastAction},
    {"ReaderModeSceneLayer", RegisterReaderModeSceneLayer},
    {"RemoteMediaPlayerBridge",
     remote_media::RemoteMediaPlayerBridge::RegisterRemoteMediaPlayerBridge},
    {"RevenueStats", RegisterRevenueStats},
    {"SavePasswordInfoBar", SavePasswordInfoBar::Register},
    {"AutoSigninFirstRunInfoBar", AutoSigninFirstRunInfoBar::Register},
    {"SceneLayer", chrome::android::RegisterSceneLayer},
    {"ScreenshotTask", RegisterScreenshotTask},
    {"ServiceTabLauncher", service_tab_launcher::RegisterServiceTabLauncherJni},
    {"SessionTabHelper", RegisterSessionTabHelper},
    {"SigninInvestigator", SigninInvestigatorAndroid::Register},
    {"SigninManager", SigninManagerAndroid::Register},
    {"SingleTabModel", RegisterSingleTabModel},
    {"SpellCheckerSessionBridge", SpellCheckerSessionBridge::RegisterJNI},
    {"SqliteCursor", SQLiteCursor::RegisterSqliteCursor},
    {"SSLClientCertificateRequest", RegisterSSLClientCertificateRequestAndroid},
    {"StartupMetricUtils", RegisterStartupMetricUtils},
    {"StaticTabSceneLayer", chrome::android::RegisterStaticTabSceneLayer},
    {"SupervisedUserContentProvider", SupervisedUserContentProvider::Register},
    {"Sync", syncer::RegisterSyncJni},
    {"TabAndroid", TabAndroid::RegisterTabAndroid},
    {"TabContentManager", chrome::android::RegisterTabContentManager},
    {"TabListSceneLayer", RegisterTabListSceneLayer},
    {"TabModelJniBridge", TabModelJniBridge::Register},
    {"TabState", RegisterTabState},
    {"TabStripSceneLayer", RegisterTabStripSceneLayer},
    {"TabWebContentsDelegateAndroid", RegisterTabWebContentsDelegateAndroid},
    {"TemplateUrlServiceAndroid", TemplateUrlServiceAndroid::Register},
    {"ToolbarModelAndroid", ToolbarModelAndroid::RegisterToolbarModelAndroid},
    {"TranslateInfoBarDelegate", RegisterTranslateInfoBarDelegate},
    {"TtsPlatformImpl", TtsPlatformImplAndroid::Register},
    {"UmaSessionStats", RegisterUmaSessionStats},
    {"UrlUtilities", RegisterUrlUtilities},
    {"Variations", variations::android::RegisterVariations},
    {"VariationsSeedBridge", variations::android::RegisterVariationsSeedBridge},
    {"VariationsSession", chrome::android::RegisterVariationsSession},
    {"WarmupManager", RegisterWarmupManager},
    {"WebappRegistry", WebappRegistry::RegisterWebappRegistry},
    {"WebContentsFactory", RegisterWebContentsFactory},
    {"WebsitePreferenceBridge", RegisterWebsitePreferenceBridge},
    {"WebsiteSettingsPopupAndroid",
     WebsiteSettingsPopupAndroid::RegisterWebsiteSettingsPopupAndroid},
#if defined(ENABLE_PRINTING) && !defined(ENABLE_PRINT_PREVIEW)
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
}  // namespace chrome
