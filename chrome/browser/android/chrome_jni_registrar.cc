// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/debug/trace_event.h"
#include "chrome/browser/android/accessibility/font_size_prefs_android.h"
#include "chrome/browser/android/accessibility_util.h"
#include "chrome/browser/android/banners/app_banner_manager.h"
#include "chrome/browser/android/bookmarks/bookmarks_bridge.h"
#include "chrome/browser/android/chrome_web_contents_delegate_android.h"
#include "chrome/browser/android/chromium_application.h"
#include "chrome/browser/android/content_view_util.h"
#include "chrome/browser/android/dev_tools_server.h"
#include "chrome/browser/android/dom_distiller/feedback_reporter_android.h"
#include "chrome/browser/android/favicon_helper.h"
#include "chrome/browser/android/foreign_session_helper.h"
#include "chrome/browser/android/intent_helper.h"
#include "chrome/browser/android/logo_bridge.h"
#include "chrome/browser/android/most_visited_sites.h"
#include "chrome/browser/android/new_tab_page_prefs.h"
#include "chrome/browser/android/omnibox/answers_image_bridge.h"
#include "chrome/browser/android/omnibox/autocomplete_controller_android.h"
#include "chrome/browser/android/omnibox/omnibox_prerender.h"
#include "chrome/browser/android/password_authentication_manager.h"
#include "chrome/browser/android/password_ui_view_android.h"
#include "chrome/browser/android/profiles/profile_downloader_android.h"
#include "chrome/browser/android/provider/chrome_browser_provider.h"
#include "chrome/browser/android/recently_closed_tabs_bridge.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/signin/account_management_screen_helper.h"
#include "chrome/browser/android/signin/signin_manager_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/uma_bridge.h"
#include "chrome/browser/android/uma_utils.h"
#include "chrome/browser/android/url_utilities.h"
#include "chrome/browser/android/voice_search_tab_helper.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory_android.h"
#include "chrome/browser/dom_distiller/tab_utils_android.h"
#include "chrome/browser/history/android/sqlite_cursor.h"
#include "chrome/browser/invalidation/invalidation_controller_android.h"
#include "chrome/browser/lifetime/application_lifetime_android.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"
#include "chrome/browser/prerender/external_prerender_handler_android.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search_engines/template_url_service_android.h"
#include "chrome/browser/signin/android_profile_oauth2_token_service.h"
#include "chrome/browser/speech/tts_android.h"
#include "chrome/browser/sync/profile_sync_service_android.h"
#include "chrome/browser/ui/android/autofill/autofill_dialog_controller_android.h"
#include "chrome/browser/ui/android/autofill/autofill_dialog_result.h"
#include "chrome/browser/ui/android/autofill/autofill_logger_android.h"
#include "chrome/browser/ui/android/autofill/autofill_popup_view_android.h"
#include "chrome/browser/ui/android/autofill/country_adapter_android.h"
#include "chrome/browser/ui/android/chrome_http_auth_handler.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/android/infobars/auto_login_infobar_delegate_android.h"
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"
#include "chrome/browser/ui/android/infobars/data_reduction_proxy_infobar.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"
#include "chrome/browser/ui/android/infobars/infobar_container_android.h"
#include "chrome/browser/ui/android/infobars/save_password_infobar.h"
#include "chrome/browser/ui/android/infobars/translate_infobar.h"
#include "chrome/browser/ui/android/javascript_app_modal_dialog_android.h"
#include "chrome/browser/ui/android/navigation_popup.h"
#include "chrome/browser/ui/android/omnibox/omnibox_view_util.h"
#include "chrome/browser/ui/android/ssl_client_certificate_request.h"
#include "chrome/browser/ui/android/tab_model/tab_model_base.h"
#include "chrome/browser/ui/android/toolbar/toolbar_model_android.h"
#include "chrome/browser/ui/android/website_settings_popup_android.h"
#include "components/bookmarks/common/android/component_jni_registrar.h"
#include "components/dom_distiller/android/component_jni_registrar.h"
#include "components/enhanced_bookmarks/android/component_jni_registrar.h"
#include "components/gcm_driver/android/component_jni_registrar.h"
#include "components/navigation_interception/component_jni_registrar.h"
#include "components/variations/android/component_jni_registrar.h"
#include "components/web_contents_delegate_android/component_jni_registrar.h"

#if defined(ENABLE_PRINTING) && !defined(ENABLE_FULL_PRINTING)
#include "printing/printing_context_android.h"
#endif

bool RegisterCertificateViewer(JNIEnv* env);

namespace chrome {
namespace android {

static base::android::RegistrationMethod kChromeRegisteredMethods[] = {
  // Register JNI for components we depend on.
  { "Bookmarks", bookmarks::android::RegisterBookmarks },
  { "DomDistiller", dom_distiller::android::RegisterDomDistiller },
  { "EnhancedBookmarks",
    enhanced_bookmarks::android::RegisterEnhancedBookmarks },
  { "GCMDriver", gcm::android::RegisterGCMDriverJni },
  { "NavigationInterception",
    navigation_interception::RegisterNavigationInterceptionJni },
  { "WebContentsDelegateAndroid",
    web_contents_delegate_android::RegisterWebContentsDelegateAndroidJni },
  // Register JNI for chrome classes.
  { "AccessibilityUtils", AccessibilityUtil::Register },
  { "AccountManagementScreenHelper", AccountManagementScreenHelper::Register },
  { "AndroidProfileOAuth2TokenService",
    AndroidProfileOAuth2TokenService::Register },
  { "AnswersImageBridge", RegisterAnswersImageBridge },
  { "AppBannerManager", banners::RegisterAppBannerManager },
  { "ApplicationLifetime", RegisterApplicationLifetimeAndroid },
  { "AutocompleteControllerAndroid", RegisterAutocompleteControllerAndroid },
  { "AutofillDialogControllerAndroid",
    autofill::AutofillDialogControllerAndroid::
        RegisterAutofillDialogControllerAndroid },
  { "AutofillDialogResult",
    autofill::AutofillDialogResult::RegisterAutofillDialogResult },
  { "AutofillLoggerAndroid",
    autofill::AutofillLoggerAndroid::Register },
  { "AutofillPopup",
    autofill::AutofillPopupViewAndroid::RegisterAutofillPopupViewAndroid },
  { "AutoLoginDelegate", AutoLoginInfoBarDelegateAndroid::Register },
  { "BookmarksBridge", BookmarksBridge::RegisterBookmarksBridge },
  { "CertificateViewer", RegisterCertificateViewer },
  { "ChromeBrowserProvider",
    ChromeBrowserProvider::RegisterChromeBrowserProvider },
  { "ChromeHttpAuthHandler",
    ChromeHttpAuthHandler::RegisterChromeHttpAuthHandler },
  { "ChromeWebContentsDelegateAndroid",
    RegisterChromeWebContentsDelegateAndroid },
  { "ChromiumApplication",
    ChromiumApplication::RegisterBindings },
  { "ConfirmInfoBarDelegate", RegisterConfirmInfoBarDelegate },
  { "ContentViewUtil", RegisterContentViewUtil },
  { "ContextMenuHelper", RegisterContextMenuHelper },
  { "CountryAdapterAndroid", autofill::CountryAdapterAndroid::Register },
  { "DataReductionProxyInfoBarDelegate", DataReductionProxyInfoBar::Register },
  { "DataReductionProxySettings", DataReductionProxySettingsAndroid::Register },
  { "DevToolsServer", RegisterDevToolsServer },
  { "DomDistillerServiceFactory",
    dom_distiller::android::DomDistillerServiceFactoryAndroid::Register },
  { "DomDistillerTabUtils", RegisterDomDistillerTabUtils },
  { "ExternalPrerenderRequestHandler",
      prerender::ExternalPrerenderHandlerAndroid::
      RegisterExternalPrerenderHandlerAndroid },
  { "FaviconHelper", FaviconHelper::RegisterFaviconHelper },
  { "FeedbackReporter", dom_distiller::android::RegisterFeedbackReporter },
  { "FontSizePrefsAndroid", FontSizePrefsAndroid::Register },
  { "ForeignSessionHelper",
    ForeignSessionHelper::RegisterForeignSessionHelper },
  { "InfoBarContainer", RegisterInfoBarContainer },
  { "ShortcutHelper", ShortcutHelper::RegisterShortcutHelper },
  { "IntentHelper", RegisterIntentHelper },
  { "InvalidationController", invalidation::RegisterInvalidationController },
  { "JavascriptAppModalDialog",
    JavascriptAppModalDialogAndroid::RegisterJavascriptAppModalDialog },
  { "LogoBridge", RegisterLogoBridge },
  { "MostVisitedSites", MostVisitedSites::Register },
  { "NativeInfoBar", RegisterNativeInfoBar },
  { "NavigationPopup", NavigationPopup::RegisterNavigationPopup },
  { "NewTabPagePrefs",
    NewTabPagePrefs::RegisterNewTabPagePrefs },
  { "OmniboxPrerender", RegisterOmniboxPrerender },
  { "OmniboxViewUtil", OmniboxViewUtil::RegisterOmniboxViewUtil },
  { "PasswordAuthenticationManager",
    PasswordAuthenticationManager::RegisterPasswordAuthenticationManager },
  { "PasswordUIViewAndroid",
    PasswordUIViewAndroid::RegisterPasswordUIViewAndroid },
  { "PersonalDataManagerAndroid",
    autofill::PersonalDataManagerAndroid::Register },
  { "ProfileAndroid", ProfileAndroid::RegisterProfileAndroid },
  { "ProfileDownloaderAndroid", ProfileDownloaderAndroid::Register },
  { "ProfileSyncService", ProfileSyncServiceAndroid::Register },
  { "RecentlyClosedBridge", RecentlyClosedTabsBridge::Register },
  { "SavePasswordInfoBar", RegisterSavePasswordInfoBar },
  { "SigninManager", SigninManagerAndroid::Register },
  { "SqliteCursor", SQLiteCursor::RegisterSqliteCursor },
  { "SSLClientCertificateRequest", RegisterSSLClientCertificateRequestAndroid },
  { "StartupMetricUtils", RegisterStartupMetricUtils },
  { "TabAndroid", TabAndroid::RegisterTabAndroid },
  { "TabModelBase", RegisterTabModelBase},
  { "TemplateUrlServiceAndroid", TemplateUrlServiceAndroid::Register },
  { "ToolbarModelAndroid", ToolbarModelAndroid::RegisterToolbarModelAndroid },
  { "TranslateInfoBarDelegate", RegisterTranslateInfoBarDelegate },
  { "TtsPlatformImpl", TtsPlatformImplAndroid::Register },
  { "UmaBridge", RegisterUmaBridge },
  { "UrlUtilities", RegisterUrlUtilities },
  { "Variations", variations::android::RegisterVariations },
  { "VoiceSearchTabHelper", RegisterVoiceSearchTabHelper },
  { "WebsiteSettingsPopupAndroid",
    WebsiteSettingsPopupAndroid::RegisterWebsiteSettingsPopupAndroid },
#if defined(ENABLE_PRINTING) && !defined(ENABLE_FULL_PRINTING)
  { "PrintingContext",
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
