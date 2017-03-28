// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_manifest_parser_android.h"

#include <stddef.h>

#include <climits>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "jni/PaymentManifestParser_jni.h"

namespace payments {
namespace {

class ParseCallback {
 public:
  explicit ParseCallback(const base::android::JavaParamRef<jobject>& jcallback)
      : jcallback_(jcallback) {}

  ~ParseCallback() {}

  void OnManifestParsed(
      std::vector<mojom::PaymentManifestSectionPtr> manifest) {
    JNIEnv* env = base::android::AttachCurrentThread();

    if (manifest.empty()) {
      // Can trigger synchronous deletion of PaymentManifestParserAndroid.
      Java_ManifestParseCallback_onManifestParseFailure(env, jcallback_);
      return;
    }

    base::android::ScopedJavaLocalRef<jobjectArray> jmanifest =
        Java_PaymentManifestParser_createManifest(env, manifest.size());

    // Java array indices must be integers.
    for (size_t i = 0; i < manifest.size() && i <= static_cast<size_t>(INT_MAX);
         ++i) {
      const mojom::PaymentManifestSectionPtr& section = manifest[i];
      if (section->sha256_cert_fingerprints.size() >
          static_cast<size_t>(INT_MAX)) {
        continue;
      }

      Java_PaymentManifestParser_addSectionToManifest(
          env, jmanifest.obj(), base::checked_cast<int>(i),
          base::android::ConvertUTF8ToJavaString(env, section->package_name),
          section->version,
          base::checked_cast<int>(section->sha256_cert_fingerprints.size()));

      for (size_t j = 0; j < section->sha256_cert_fingerprints.size() &&
                         j <= static_cast<size_t>(INT_MAX);
           ++j) {
        const std::vector<uint8_t>& fingerprint =
            section->sha256_cert_fingerprints[j];
        Java_PaymentManifestParser_addFingerprintToSection(
            env, jmanifest.obj(), base::checked_cast<int>(i),
            base::checked_cast<int>(j),
            base::android::ToJavaByteArray(env, fingerprint));
      }
    }

    // Can trigger synchronous deletion of PaymentManifestParserAndroid.
    Java_ManifestParseCallback_onManifestParseSuccess(env, jcallback_,
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

void PaymentManifestParserAndroid::ParsePaymentManifest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jcontent,
    const base::android::JavaParamRef<jobject>& jcallback) {
  host_.ParsePaymentManifest(
      base::android::ConvertJavaStringToUTF8(env, jcontent),
      base::BindOnce(&ParseCallback::OnManifestParsed,
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
