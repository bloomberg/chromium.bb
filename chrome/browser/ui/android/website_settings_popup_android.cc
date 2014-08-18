// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/website_settings_popup_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ui/website_settings/website_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/ssl_status.h"
#include "grit/generated_resources.h"
#include "jni/WebsiteSettingsPopup_jni.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::CheckException;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::GetClass;
using base::android::ScopedJavaLocalRef;
using content::CertStore;
using content::WebContents;

static jobjectArray GetCertificateChain(JNIEnv* env,
                                        jobject obj,
                                        jobject java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  if (!web_contents)
    return NULL;

  int cert_id =
      web_contents->GetController().GetVisibleEntry()->GetSSL().cert_id;
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
static jlong Init(JNIEnv* env,
                  jclass clazz,
                  jobject obj,
                  jobject java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);

  return reinterpret_cast<intptr_t>(
      new WebsiteSettingsPopupAndroid(env, obj, web_contents));
}

WebsiteSettingsPopupAndroid::WebsiteSettingsPopupAndroid(
    JNIEnv* env,
    jobject java_website_settings_pop,
    WebContents* web_contents) {
  // Important to use GetVisibleEntry to match what's showing in the omnibox.
  content::NavigationEntry* nav_entry =
      web_contents->GetController().GetVisibleEntry();
  if (nav_entry == NULL)
    return;

  popup_jobject_.Reset(env, java_website_settings_pop);

  presenter_.reset(new WebsiteSettings(
      this,
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      TabSpecificContentSettings::FromWebContents(web_contents),
      InfoBarService::FromWebContents(web_contents),
      nav_entry->GetURL(),
      nav_entry->GetSSL(),
      content::CertStore::GetInstance()));
}

WebsiteSettingsPopupAndroid::~WebsiteSettingsPopupAndroid() {}

void WebsiteSettingsPopupAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void WebsiteSettingsPopupAndroid::ResetCertDecisions(
    JNIEnv* env,
    jobject obj,
    jobject java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  if (!web_contents)
    return;
  ChromeSSLHostStateDelegate* delegate =
      presenter_->chrome_ssl_host_state_delegate();
  DCHECK(delegate);
  delegate->RevokeUserDecisionsHard(presenter_->site_url().host());
}

void WebsiteSettingsPopupAndroid::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  JNIEnv* env = base::android::AttachCurrentThread();

  {
    int icon_id = ResourceMapper::MapFromChromiumId(
        WebsiteSettingsUI::GetIdentityIconID(identity_info.identity_status));

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
    base::string16 certificate_label =
        l10n_util::GetStringUTF16(IDS_PAGEINFO_CERT_INFO_BUTTON);
    Java_WebsiteSettingsPopup_addCertificateSection(
        env,
        popup_jobject_.obj(),
        icon_id,
        ConvertUTF8ToJavaString(env, headline).obj(),
        description.obj(),
        ConvertUTF16ToJavaString(env, certificate_label).obj());

    if (identity_info.show_ssl_decision_revoke_button) {
      base::string16 reset_button_label = l10n_util::GetStringUTF16(
          IDS_PAGEINFO_RESET_INVALID_CERTIFICATE_DECISIONS_BUTTON);
      Java_WebsiteSettingsPopup_addResetCertDecisionsButton(
          env,
          popup_jobject_.obj(),
          ConvertUTF16ToJavaString(env, reset_button_label).obj());
    }
  }

  {
    int icon_id = ResourceMapper::MapFromChromiumId(
        WebsiteSettingsUI::GetConnectionIconID(
            identity_info.connection_status));

    ScopedJavaLocalRef<jstring> description = ConvertUTF8ToJavaString(
        env, identity_info.connection_status_description);
    Java_WebsiteSettingsPopup_addDescriptionSection(
        env, popup_jobject_.obj(), icon_id, NULL, description.obj());
  }

  Java_WebsiteSettingsPopup_addMoreInfoLink(env, popup_jobject_.obj(),
      ConvertUTF8ToJavaString(
          env, l10n_util::GetStringUTF8(IDS_PAGE_INFO_HELP_CENTER_LINK)).obj());
  Java_WebsiteSettingsPopup_showDialog(env, popup_jobject_.obj());
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
    const base::string16& first_visit) {
  NOTIMPLEMENTED();
}

// static
bool WebsiteSettingsPopupAndroid::RegisterWebsiteSettingsPopupAndroid(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}
