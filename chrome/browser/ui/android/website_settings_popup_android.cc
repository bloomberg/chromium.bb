// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/website_settings_popup_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/website_settings/website_settings.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/ssl_status.h"
#include "grit/generated_resources.h"
#include "jni/WebsiteSettingsPopup_jni.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::CheckException;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::GetClass;
using base::android::ScopedJavaLocalRef;
using content::CertStore;
using content::WebContents;
using gfx::ConvertToJavaBitmap;

static jobjectArray GetCertificateChain(JNIEnv* env,
                                        jobject obj,
                                        jobject view) {
  content::WebContents* contents =
      content::ContentViewCore::GetNativeContentViewCore(env, view)->
          GetWebContents();
  int cert_id = contents->GetController().GetActiveEntry()->GetSSL().cert_id;
  scoped_refptr<net::X509Certificate> cert;
  bool ok = CertStore::GetInstance()->RetrieveCert(cert_id, &cert);
  CHECK(ok);

  std::vector<std::string> cert_chain;
  net::X509Certificate::OSCertHandles cert_handles =
      cert->GetIntermediateCertificates();
  // Make sure the peer's own cert is the first in the chain, if it's not
  // already there.
  if (cert_handles.empty() || cert_handles[0] != cert->os_cert_handle())
    cert_handles.insert(cert_handles.begin(), cert->os_cert_handle());

  cert_chain.reserve(cert_handles.size());
  for (net::X509Certificate::OSCertHandles::const_iterator it =
           cert_handles.begin();
       it != cert_handles.end();
       ++it) {
    std::string cert_bytes;
    net::X509Certificate::GetDEREncoded(*it, &cert_bytes);
    cert_chain.push_back(cert_bytes);
  }

  // OK to release, JNI binding.
  return base::android::ToJavaArrayOfByteArray(env, cert_chain).Release();
}

// static
void WebsiteSettingsPopupAndroid::Show(JNIEnv* env,
                                       jobject context,
                                       jobject java_content_view,
                                       WebContents* web_contents) {
  new WebsiteSettingsPopupAndroid(env, context, java_content_view,
                                  web_contents);
}

WebsiteSettingsPopupAndroid::WebsiteSettingsPopupAndroid(
    JNIEnv* env,
    jobject context,
    jobject java_content_view,
    WebContents* web_contents) {
  if (web_contents->GetController().GetActiveEntry() == NULL)
    return;

  popup_jobject_.Reset(
      Java_WebsiteSettingsPopup_create(env, context, java_content_view,
                                       reinterpret_cast<jint>(this)));

  presenter_.reset(new WebsiteSettings(
      this,
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      TabSpecificContentSettings::FromWebContents(web_contents),
      InfoBarService::FromWebContents(web_contents),
      web_contents->GetURL(),
      web_contents->GetController().GetActiveEntry()->GetSSL(),
      content::CertStore::GetInstance()));
}

WebsiteSettingsPopupAndroid::~WebsiteSettingsPopupAndroid() {}

void WebsiteSettingsPopupAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void WebsiteSettingsPopupAndroid::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  JNIEnv* env = base::android::AttachCurrentThread();

  {
    const gfx::Image& icon_image = WebsiteSettingsUI::GetIdentityIcon(
        identity_info.identity_status);
    // Creates a java version of the bitmap and makes a copy of the pixels
    ScopedJavaLocalRef<jobject> icon = ConvertToJavaBitmap(
        icon_image.ToSkBitmap());

    // The headline and the certificate dialog link of the site's identity
    // section is only displayed if the site's identity was verified. If the
    // site's identity was verified, then the headline contains the organization
    // name from the provided certificate. If the organization name is not
    // available than the hostname of the site is used instead.
    std::string headline;
    if (identity_info.cert_id) {
      headline = identity_info.site_identity;
    }

    ScopedJavaLocalRef<jstring> description = ConvertUTF8ToJavaString(
        env, identity_info.identity_status_description);
    Java_WebsiteSettingsPopup_addSection(env, popup_jobject_.obj(), icon.obj(),
        ConvertUTF8ToJavaString(env, headline).obj(), description.obj());

    string16 certificate_label =
        l10n_util::GetStringUTF16(IDS_PAGEINFO_CERT_INFO_BUTTON);
    if (!certificate_label.empty()) {
      Java_WebsiteSettingsPopup_setCertificateViewer(env, popup_jobject_.obj(),
          ConvertUTF16ToJavaString(env, certificate_label).obj());
    }

    Java_WebsiteSettingsPopup_addDivider(env, popup_jobject_.obj());
  }

  {
     const gfx::Image& icon_image = WebsiteSettingsUI::GetConnectionIcon(
         identity_info.connection_status);
     ScopedJavaLocalRef<jobject> icon = ConvertToJavaBitmap(
         icon_image.ToSkBitmap());

     ScopedJavaLocalRef<jstring> description = ConvertUTF8ToJavaString(
         env, identity_info.connection_status_description);
     Java_WebsiteSettingsPopup_addSection(env, popup_jobject_.obj(), icon.obj(),
         NULL, description.obj());

     Java_WebsiteSettingsPopup_addDivider(env, popup_jobject_.obj());
  }

  Java_WebsiteSettingsPopup_addMoreInfoLink(env, popup_jobject_.obj(),
      ConvertUTF8ToJavaString(
          env, l10n_util::GetStringUTF8(IDS_PAGE_INFO_HELP_CENTER_LINK)).obj());
  Java_WebsiteSettingsPopup_show(env, popup_jobject_.obj());
}

void WebsiteSettingsPopupAndroid::SetCookieInfo(
    const CookieInfoList& cookie_info_list) {
  NOTIMPLEMENTED();
}

void WebsiteSettingsPopupAndroid::SetPermissionInfo(
    const PermissionInfoList& permission_info_list) {
  NOTIMPLEMENTED();
}

void WebsiteSettingsPopupAndroid::SetSelectedTab(
    WebsiteSettingsUI::TabId tab_id) {
  // There's no tab UI on Android - only connection info is shown.
  NOTIMPLEMENTED();
}

void WebsiteSettingsPopupAndroid::SetFirstVisit(
    const string16& first_visit) {
  NOTIMPLEMENTED();
}

// static
bool WebsiteSettingsPopupAndroid::RegisterWebsiteSettingsPopupAndroid(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}
