// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_manifest_parser_android.h"

#include <stddef.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "jni/PaymentManifestParser_jni.h"
#include "url/gurl.h"

namespace payments {
namespace {

class ParseCallback {
 public:
  explicit ParseCallback(const base::android::JavaParamRef<jobject>& jcallback)
      : jcallback_(jcallback) {}

  ~ParseCallback() {}

  // Copies payment method manifest into Java.
  void OnPaymentMethodManifestParsed(
      const std::vector<GURL>& web_app_manifest_urls,
      const std::vector<url::Origin>& unused_supported_origins,
      bool unused_all_origins_supported) {
    DCHECK_GE(100U, web_app_manifest_urls.size());
    JNIEnv* env = base::android::AttachCurrentThread();

    if (web_app_manifest_urls.empty()) {
      // Can trigger synchronous deletion of PaymentManifestParserAndroid.
      Java_ManifestParseCallback_onManifestParseFailure(env, jcallback_);
      return;
    }

    base::android::ScopedJavaLocalRef<jobjectArray> juris =
        Java_PaymentManifestParser_createWebAppManifestUris(
            env, web_app_manifest_urls.size());

    for (size_t i = 0; i < web_app_manifest_urls.size(); ++i) {
      bool is_valid_uri = Java_PaymentManifestParser_addUri(
          env, juris.obj(), base::checked_cast<int>(i),
          base::android::ConvertUTF8ToJavaString(
              env, web_app_manifest_urls[i].spec()));
      DCHECK(is_valid_uri);
    }

    // Can trigger synchronous deletion of PaymentManifestParserAndroid.
    Java_ManifestParseCallback_onPaymentMethodManifestParseSuccess(
        env, jcallback_, juris.obj());
  }

  // Copies web app manifest into Java.
  void OnWebAppManifestParsed(
      std::vector<mojom::WebAppManifestSectionPtr> manifest) {
    DCHECK_GE(100U, manifest.size());
    JNIEnv* env = base::android::AttachCurrentThread();

    if (manifest.empty()) {
      // Can trigger synchronous deletion of PaymentManifestParserAndroid.
      Java_ManifestParseCallback_onManifestParseFailure(env, jcallback_);
      return;
    }

    base::android::ScopedJavaLocalRef<jobjectArray> jmanifest =
        Java_PaymentManifestParser_createManifest(env, manifest.size());

    for (size_t i = 0; i < manifest.size(); ++i) {
      const mojom::WebAppManifestSectionPtr& section = manifest[i];
      DCHECK_GE(100U, section->fingerprints.size());

      Java_PaymentManifestParser_addSectionToManifest(
          env, jmanifest.obj(), base::checked_cast<int>(i),
          base::android::ConvertUTF8ToJavaString(env, section->id),
          section->min_version,
          base::checked_cast<int>(section->fingerprints.size()));

      for (size_t j = 0; j < section->fingerprints.size(); ++j) {
        const std::vector<uint8_t>& fingerprint = section->fingerprints[j];
        Java_PaymentManifestParser_addFingerprintToSection(
            env, jmanifest.obj(), base::checked_cast<int>(i),
            base::checked_cast<int>(j),
            base::android::ToJavaByteArray(env, fingerprint));
      }
    }

    // Can trigger synchronous deletion of PaymentManifestParserAndroid.
    Java_ManifestParseCallback_onWebAppManifestParseSuccess(env, jcallback_,
                                                            jmanifest.obj());
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> jcallback_;

  DISALLOW_COPY_AND_ASSIGN(ParseCallback);
};

}  // namespace

PaymentManifestParserAndroid::PaymentManifestParserAndroid() {}

PaymentManifestParserAndroid::~PaymentManifestParserAndroid() {}

void PaymentManifestParserAndroid::StartUtilityProcess(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  host_.StartUtilityProcess();
}

void PaymentManifestParserAndroid::ParsePaymentMethodManifest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jcontent,
    const base::android::JavaParamRef<jobject>& jcallback) {
  host_.ParsePaymentMethodManifest(
      base::android::ConvertJavaStringToUTF8(env, jcontent),
      base::BindOnce(&ParseCallback::OnPaymentMethodManifestParsed,
                     base::MakeUnique<ParseCallback>(jcallback)));
}

void PaymentManifestParserAndroid::ParseWebAppManifest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jcontent,
    const base::android::JavaParamRef<jobject>& jcallback) {
  host_.ParseWebAppManifest(
      base::android::ConvertJavaStringToUTF8(env, jcontent),
      base::BindOnce(&ParseCallback::OnWebAppManifestParsed,
                     base::MakeUnique<ParseCallback>(jcallback)));
}

void PaymentManifestParserAndroid::StopUtilityProcess(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  delete this;
}

bool RegisterPaymentManifestParser(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// Caller owns the result.
jlong CreatePaymentManifestParserAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller) {
  return reinterpret_cast<jlong>(new PaymentManifestParserAndroid);
}

}  // namespace payments
