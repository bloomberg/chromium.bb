// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/toolbar_model_android.h"

#include "base/android/jni_string.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/ssl_status.h"
#include "jni/ToolbarModel_jni.h"
#include "net/cert/x509_certificate.h"

using base::android::ScopedJavaLocalRef;

ToolbarModelAndroid::ToolbarModelAndroid(JNIEnv* env, jobject jdelegate)
    : toolbar_model_(new ToolbarModelImpl(this)),
      weak_java_delegate_(env, jdelegate) {
}

ToolbarModelAndroid::~ToolbarModelAndroid() {
}

void ToolbarModelAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

ScopedJavaLocalRef<jstring> ToolbarModelAndroid::GetText(JNIEnv* env,
                                                         jobject obj) {
  return base::android::ConvertUTF16ToJavaString(env,
                                                 toolbar_model_->GetText());
}

ScopedJavaLocalRef<jstring> ToolbarModelAndroid::GetCorpusChipText(
    JNIEnv* env,
    jobject obj) {
  return base::android::ConvertUTF16ToJavaString(
      env,
      toolbar_model_->GetCorpusNameForMobile());
}

jboolean ToolbarModelAndroid::WouldReplaceURL(JNIEnv* env, jobject obj) {
  return toolbar_model_->WouldReplaceURL();
}

content::WebContents* ToolbarModelAndroid::GetActiveWebContents() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jdelegate = weak_java_delegate_.get(env);
  if (!jdelegate.obj())
    return NULL;
  ScopedJavaLocalRef<jobject> jweb_contents =
      Java_ToolbarModelDelegate_getActiveWebContents(env, jdelegate.obj());
  return content::WebContents::FromJavaWebContents(jweb_contents.obj());
}

// static
bool ToolbarModelAndroid::RegisterToolbarModelAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
jlong Init(JNIEnv* env, jobject obj, jobject delegate) {
  ToolbarModelAndroid* toolbar_model = new ToolbarModelAndroid(env, delegate);
  return reinterpret_cast<intptr_t>(toolbar_model);
}

// static
jint GetSecurityLevelForWebContents(JNIEnv* env,
                                    jclass jcaller,
                                    jobject jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  return ToolbarModelImpl::GetSecurityLevelForWebContents(web_contents);
}

// Temporary method to allow us to surface a SHA-1 deprecation string on Android
// in M42. This duplicates a subset of the logic from
// ToolbarModelImpl::GetSecurityLevelForWebContents() and
// WebsiteSettings::Init(), which should really be refactored.
// This is at the wrong layer, and needs to be refactored (along with desktop):
// https://crbug.com/471390

// static
jboolean IsDeprecatedSHA1Present(JNIEnv* env,
                                 jclass jcaller,
                                 jobject jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  if (!entry)
    return false;

  const content::SSLStatus& ssl = entry->GetSSL();
  if (ssl.security_style == content::SECURITY_STYLE_AUTHENTICATED) {
    scoped_refptr<net::X509Certificate> cert;
    // NOTE: This constant needs to be kept in sync with
    // ToolbarModelImpl::GetSecurityLevelForWebContents().
    static const int64_t kJanuary2016 = INT64_C(13096080000000000);
    if (content::CertStore::GetInstance()->RetrieveCert(ssl.cert_id, &cert) &&
        (ssl.cert_status & net::CERT_STATUS_SHA1_SIGNATURE_PRESENT) &&
        cert->valid_expiry() > base::Time::FromInternalValue(kJanuary2016)) {
      return true;
    }
  }
  return false;
}
