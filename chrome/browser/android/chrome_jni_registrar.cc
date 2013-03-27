// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "chrome/browser/android/chrome_web_contents_delegate_android.h"
#include "chrome/browser/android/content_view_util.h"
#include "chrome/browser/android/dev_tools_server.h"
#include "chrome/browser/android/intent_helper.h"
#include "chrome/browser/android/process_utils.h"
#include "chrome/browser/android/provider/chrome_browser_provider.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/history/android/sqlite_cursor.h"
#include "chrome/browser/lifetime/application_lifetime_android.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search_engines/template_url_service_android.h"
#include "chrome/browser/sync/profile_sync_service_android.h"
#include "chrome/browser/ui/android/autofill/autofill_dialog_view_android.h"
#include "chrome/browser/ui/android/autofill/autofill_popup_view_android.h"
#include "chrome/browser/ui/android/chrome_http_auth_handler.h"
#include "chrome/browser/ui/android/javascript_app_modal_dialog_android.h"
#include "chrome/browser/ui/android/navigation_popup.h"
#include "chrome/browser/ui/android/ssl_client_certificate_request.h"
#include "chrome/browser/ui/android/website_settings_popup_android.h"
#include "components/autofill/browser/android/component_jni_registrar.h"
#include "components/navigation_interception/component_jni_registrar.h"
#include "components/web_contents_delegate_android/component_jni_registrar.h"

bool RegisterCertificateViewer(JNIEnv* env);

namespace chrome {
namespace android {

static base::android::RegistrationMethod kChromeRegisteredMethods[] = {
  // Register JNI for components we depend on.
  { "NavigationInterception", components::RegisterNavigationInterceptionJni },
  { "WebContentsDelegateAndroid",
      components::RegisterWebContentsDelegateAndroidJni },
  { "RegisterAuxiliaryProfileLoader",
      components::RegisterAutofillAndroidJni },
  // Register JNI for chrome classes.
  { "ApplicationLifetime", RegisterApplicationLifetimeAndroid},
  { "AutofillDialog",
      autofill::AutofillDialogViewAndroid::RegisterAutofillDialogViewAndroid},
  { "AutofillPopup",
      AutofillPopupViewAndroid::RegisterAutofillPopupViewAndroid},
  { "CertificateViewer", RegisterCertificateViewer},
  { "ChromeBrowserProvider",
      ChromeBrowserProvider::RegisterChromeBrowserProvider },
  { "ChromeHttpAuthHandler",
      ChromeHttpAuthHandler::RegisterChromeHttpAuthHandler },
  { "ChromeWebContentsDelegateAndroid",
      RegisterChromeWebContentsDelegateAndroid },
  { "ContentViewUtil", RegisterContentViewUtil },
  { "DevToolsServer", RegisterDevToolsServer },
  { "IntentHelper", RegisterIntentHelper },
  { "JavascriptAppModalDialog",
      JavascriptAppModalDialogAndroid::RegisterJavascriptAppModalDialog },
  { "NavigationPopup", NavigationPopup::RegisterNavigationPopup },
  { "PersonalDataManagerAndroid",
      PersonalDataManagerAndroid::Register},
  { "ProcessUtils", RegisterProcessUtils },
  { "ProfileAndroid", ProfileAndroid::RegisterProfileAndroid },
  { "ProfileSyncService", ProfileSyncServiceAndroid::Register },
  { "SqliteCursor", SQLiteCursor::RegisterSqliteCursor },
  { "SSLClientCertificateRequest",
      RegisterSSLClientCertificateRequestAndroid },
  { "TemplateUrlServiceAndroid", TemplateUrlServiceAndroid::Register },
  { "WebsiteSettingsPopupAndroid",
      WebsiteSettingsPopupAndroid::RegisterWebsiteSettingsPopupAndroid },
};

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kChromeRegisteredMethods,
                               arraysize(kChromeRegisteredMethods));
}

} // namespace android
} // namespace chrome
