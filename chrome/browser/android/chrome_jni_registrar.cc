// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/accessibility/font_size_prefs_android.h"
#include "chrome/browser/android/accessibility_util.h"
#include "chrome/browser/android/appmenu/app_menu_drag_helper.h"
#include "chrome/browser/android/banners/app_banner_infobar_delegate.h"
#include "chrome/browser/android/banners/app_banner_manager.h"
#include "chrome/browser/android/bookmarks/bookmarks_bridge.h"
#include "chrome/browser/android/bookmarks/partner_bookmarks_reader.h"
#include "chrome/browser/android/chrome_web_contents_delegate_android.h"
#include "chrome/browser/android/chromium_application.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"
#include "chrome/browser/android/compositor/scene_layer/static_tab_scene_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "chrome/browser/android/content_view_util.h"
#include "chrome/browser/android/cookies/cookies_fetcher.h"
#include "chrome/browser/android/dev_tools_server.h"
#include "chrome/browser/android/dom_distiller/feedback_reporter_android.h"
#include "chrome/browser/android/download/chrome_download_delegate.h"
#include "chrome/browser/android/favicon_helper.h"
#include "chrome/browser/android/feature_utilities.h"
#include "chrome/browser/android/find_in_page/find_in_page_bridge.h"
#include "chrome/browser/android/foreign_session_helper.h"
#include "chrome/browser/android/intent_helper.h"
#include "chrome/browser/android/location_settings_impl.h"
#include "chrome/browser/android/logo_bridge.h"
#include "chrome/browser/android/metrics/uma_session_stats.h"
#include "chrome/browser/android/most_visited_sites.h"
#include "chrome/browser/android/new_tab_page_prefs.h"
#include "chrome/browser/android/omnibox/answers_image_bridge.h"
#include "chrome/browser/android/omnibox/autocomplete_controller_android.h"
#include "chrome/browser/android/omnibox/omnibox_prerender.h"
#include "chrome/browser/android/password_ui_view_android.h"
#include "chrome/browser/android/preferences/autofill/autofill_profile_bridge.h"
#include "chrome/browser/android/preferences/pref_service_bridge.h"
#include "chrome/browser/android/preferences/website_preference_bridge.h"
#include "chrome/browser/android/profiles/profile_downloader_android.h"
#include "chrome/browser/android/provider/chrome_browser_provider.h"
#include "chrome/browser/android/recently_closed_tabs_bridge.h"
#include "chrome/browser/android/service_tab_launcher.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/signin/account_management_screen_helper.h"
#include "chrome/browser/android/signin/signin_manager_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_state.h"
#include "chrome/browser/android/uma_bridge.h"
#include "chrome/browser/android/uma_utils.h"
#include "chrome/browser/android/url_utilities.h"
#include "chrome/browser/android/voice_search_tab_helper.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory_android.h"
#include "chrome/browser/dom_distiller/tab_utils_android.h"
#include "chrome/browser/enhanced_bookmarks/android/enhanced_bookmarks_bridge.h"
#include "chrome/browser/history/android/sqlite_cursor.h"
#include "chrome/browser/invalidation/invalidation_service_factory_android.h"
#include "chrome/browser/lifetime/application_lifetime_android.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"
#include "chrome/browser/notifications/notification_ui_manager_android.h"
#include "chrome/browser/prerender/external_prerender_handler_android.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search_engines/template_url_service_android.h"
#include "chrome/browser/signin/android_profile_oauth2_token_service.h"
#include "chrome/browser/speech/tts_android.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_feedback_reporter_android.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_android.h"
#include "chrome/browser/sync/profile_sync_service_android.h"
#include "chrome/browser/ui/android/autofill/autofill_dialog_controller_android.h"
#include "chrome/browser/ui/android/autofill/autofill_dialog_result.h"
#include "chrome/browser/ui/android/autofill/autofill_logger_android.h"
#include "chrome/browser/ui/android/autofill/autofill_popup_view_android.h"
#include "chrome/browser/ui/android/autofill/card_unmask_prompt_view_android.h"
#include "chrome/browser/ui/android/autofill/credit_card_scanner_view_android.h"
#include "chrome/browser/ui/android/autofill/password_generation_popup_view_android.h"
#include "chrome/browser/ui/android/chrome_http_auth_handler.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/android/infobars/account_chooser_infobar.h"
#include "chrome/browser/ui/android/infobars/app_banner_infobar.h"
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"
#include "chrome/browser/ui/android/infobars/data_reduction_proxy_infobar.h"
#include "chrome/browser/ui/android/infobars/generated_password_saved_infobar.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"
#include "chrome/browser/ui/android/infobars/infobar_container_android.h"
#include "chrome/browser/ui/android/infobars/translate_infobar.h"
#include "chrome/browser/ui/android/javascript_app_modal_dialog_android.h"
#include "chrome/browser/ui/android/navigation_popup.h"
#include "chrome/browser/ui/android/omnibox/omnibox_url_emphasizer.h"
#include "chrome/browser/ui/android/omnibox/omnibox_view_util.h"
#include "chrome/browser/ui/android/ssl_client_certificate_request.h"
#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"
#include "chrome/browser/ui/android/toolbar/toolbar_model_android.h"
#include "chrome/browser/ui/android/website_settings_popup_android.h"
#include "components/bookmarks/common/android/component_jni_registrar.h"
#include "components/dom_distiller/android/component_jni_registrar.h"
#include "components/gcm_driver/android/component_jni_registrar.h"
#include "components/invalidation/android/component_jni_registrar.h"
#include "components/navigation_interception/component_jni_registrar.h"
#include "components/variations/android/component_jni_registrar.h"
#include "components/web_contents_delegate_android/component_jni_registrar.h"

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
    {"ChromeDownloadDelegate", RegisterChromeDownloadDeleagte},
    {"GCMDriver", gcm::android::RegisterGCMDriverJni},
    {"Invalidation", invalidation::android::RegisterInvalidationJni},
    {"NavigationInterception",
     navigation_interception::RegisterNavigationInterceptionJni},
    {"WebContentsDelegateAndroid",
     web_contents_delegate_android::RegisterWebContentsDelegateAndroidJni},
    // Register JNI for chrome classes.
    {"AccessibilityUtils", AccessibilityUtil::Register},
    {"AccountChooserInfoBar", RegisterAccountChooserInfoBar},
    {"AccountManagementScreenHelper", AccountManagementScreenHelper::Register},
    {"AndroidProfileOAuth2TokenService",
     AndroidProfileOAuth2TokenService::Register},
    {"AnswersImageBridge", RegisterAnswersImageBridge},
    {"AppBannerInfoBar", RegisterAppBannerInfoBar},
    {"AppBannerInfoBarDelegate", banners::RegisterAppBannerInfoBarDelegate},
    {"AppBannerManager", banners::RegisterAppBannerManager},
    {"ApplicationLifetime", RegisterApplicationLifetimeAndroid},
    {"AutocompleteControllerAndroid", RegisterAutocompleteControllerAndroid},
    {"AutofillDialogControllerAndroid",
     autofill::AutofillDialogControllerAndroid::
         RegisterAutofillDialogControllerAndroid},
    {"AutofillDialogResult",
     autofill::AutofillDialogResult::RegisterAutofillDialogResult},
    {"AutofillLoggerAndroid", autofill::AutofillLoggerAndroid::Register},
    {"AutofillPopup",
     autofill::AutofillPopupViewAndroid::RegisterAutofillPopupViewAndroid},
    {"AutofillProfileBridge", autofill::RegisterAutofillProfileBridge},
    {"BookmarksBridge", BookmarksBridge::RegisterBookmarksBridge},
    {"CardUnmaskPrompt", autofill::CardUnmaskPromptViewAndroid::Register},
    {"ChildAccountFeedbackReporter", RegisterChildAccountFeedbackReporter},
    {"ChildAccountService", RegisterChildAccountService},
    {"ChromeBrowserProvider",
     ChromeBrowserProvider::RegisterChromeBrowserProvider},
    {"ChromeHttpAuthHandler",
     ChromeHttpAuthHandler::RegisterChromeHttpAuthHandler},
    {"ChromeWebContentsDelegateAndroid",
     RegisterChromeWebContentsDelegateAndroid},
    {"ChromiumApplication", ChromiumApplication::RegisterBindings},
    {"ConfirmInfoBarDelegate", RegisterConfirmInfoBarDelegate},
    {"ContentViewUtil", RegisterContentViewUtil},
    {"ContextMenuHelper", RegisterContextMenuHelper},
    {"CookiesFetcher", RegisterCookiesFetcher},
    {"CreditCardScanner", autofill::CreditCardScannerViewAndroid::Register},
    {"DataReductionProxyInfoBarDelegate", DataReductionProxyInfoBar::Register},
    {"DataReductionProxySettings", DataReductionProxySettingsAndroid::Register},
    {"DevToolsServer", RegisterDevToolsServer},
    {"DomDistillerServiceFactory",
     dom_distiller::android::DomDistillerServiceFactoryAndroid::Register},
    {"DomDistillerTabUtils", RegisterDomDistillerTabUtils},
    {"EnhancedBookmarksBridge",
     enhanced_bookmarks::android::RegisterEnhancedBookmarksBridge},
    {"ExternalPrerenderRequestHandler",
     prerender::ExternalPrerenderHandlerAndroid::
         RegisterExternalPrerenderHandlerAndroid},
    {"FaviconHelper", FaviconHelper::RegisterFaviconHelper},
    {"FeatureUtilities", RegisterFeatureUtilities},
    {"FeedbackReporter", dom_distiller::android::RegisterFeedbackReporter},
    {"FindInPageBridge", FindInPageBridge::RegisterFindInPageBridge},
    {"FontSizePrefsAndroid", FontSizePrefsAndroid::Register},
    {"ForeignSessionHelper",
     ForeignSessionHelper::RegisterForeignSessionHelper},
    {"GeneratedPasswordSavedInfoBarDelegate",
     RegisterGeneratedPasswordSavedInfoBarDelegate},
    {"InfoBarContainer", RegisterInfoBarContainer},
    {"InvalidationServiceFactory",
     invalidation::InvalidationServiceFactoryAndroid::Register},
    {"ShortcutHelper", ShortcutHelper::RegisterShortcutHelper},
    {"IntentHelper", RegisterIntentHelper},
    {"JavascriptAppModalDialog",
     JavascriptAppModalDialogAndroid::RegisterJavascriptAppModalDialog},
    {"LayerTitleCache", chrome::android::RegisterLayerTitleCache},
    {"LocationSettings", LocationSettingsImpl::Register},
    {"LogoBridge", RegisterLogoBridge},
    {"MostVisitedSites", MostVisitedSites::Register},
    {"NativeInfoBar", RegisterNativeInfoBar},
    {"NavigationPopup", NavigationPopup::RegisterNavigationPopup},
    {"NewTabPagePrefs", NewTabPagePrefs::RegisterNewTabPagePrefs},
    {"NotificationUIManager",
     NotificationUIManagerAndroid::RegisterNotificationUIManager},
    {"OmniboxPrerender", RegisterOmniboxPrerender},
    {"OmniboxUrlEmphasizer",
     OmniboxUrlEmphasizer::RegisterOmniboxUrlEmphasizer},
    {"OmniboxViewUtil", OmniboxViewUtil::RegisterOmniboxViewUtil},
    {"PartnerBookmarksReader",
     PartnerBookmarksReader::RegisterPartnerBookmarksReader},
    {"PasswordGenerationPopup",
     autofill::PasswordGenerationPopupViewAndroid::Register},
    {"PasswordUIViewAndroid",
     PasswordUIViewAndroid::RegisterPasswordUIViewAndroid},
    {"PersonalDataManagerAndroid",
     autofill::PersonalDataManagerAndroid::Register},
    {"PrefServiceBridge", RegisterPrefServiceBridge},
    {"ProfileAndroid", ProfileAndroid::RegisterProfileAndroid},
    {"ProfileDownloader", RegisterProfileDownloader},
    {"ProfileSyncService", ProfileSyncServiceAndroid::Register},
    {"RecentlyClosedBridge", RecentlyClosedTabsBridge::Register},
    {"SceneLayer", chrome::android::RegisterSceneLayer},
    {"ServiceTabLauncher", ServiceTabLauncher::RegisterServiceTabLauncher},
    {"SigninManager", SigninManagerAndroid::Register},
    {"SqliteCursor", SQLiteCursor::RegisterSqliteCursor},
    {"SSLClientCertificateRequest", RegisterSSLClientCertificateRequestAndroid},
    {"StartupMetricUtils", RegisterStartupMetricUtils},
    {"StaticTabSceneLayer", chrome::android::RegisterStaticTabSceneLayer},
    {"TabAndroid", TabAndroid::RegisterTabAndroid},
    {"TabContentManager", chrome::android::RegisterTabContentManager},
    {"TabModelJniBridge", TabModelJniBridge::Register},
    {"TabState", RegisterTabState},
    {"TemplateUrlServiceAndroid", TemplateUrlServiceAndroid::Register},
    {"ToolbarModelAndroid", ToolbarModelAndroid::RegisterToolbarModelAndroid},
    {"TranslateInfoBarDelegate", RegisterTranslateInfoBarDelegate},
    {"TtsPlatformImpl", TtsPlatformImplAndroid::Register},
    {"UmaBridge", RegisterUmaBridge},
    {"UmaSessionStats", RegisterUmaSessionStats},
    {"UrlUtilities", RegisterUrlUtilities},
    {"Variations", variations::android::RegisterVariations},
    {"VoiceSearchTabHelper", RegisterVoiceSearchTabHelper},
    {"WebsitePreferenceBridge", RegisterWebsitePreferenceBridge},
    {"WebsiteSettingsPopupAndroid",
     WebsiteSettingsPopupAndroid::RegisterWebsiteSettingsPopupAndroid},
#if defined(ENABLE_PRINTING) && !defined(ENABLE_PRINT_PREVIEW)
    {"PrintingContext",
     printing::PrintingContextAndroid::RegisterPrintingContext},
#endif
};

bool RegisterJni(JNIEnv* env) {
  TRACE_EVENT0("startup", "chrome_android::RegisterJni");
  return RegisterNativeMethods(env, kChromeRegisteredMethods,
                               arraysize(kChromeRegisteredMethods));
}

}  // namespace android
}  // namespace chrome
