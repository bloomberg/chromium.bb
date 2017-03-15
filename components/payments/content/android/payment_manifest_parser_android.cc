// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_manifest_parser_android.h"

#include <stddef.h>

#include <climits>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/utility_process_mojo_client.h"
#include "jni/PaymentManifestParser_jni.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

class PaymentManifestParserAndroid::ParseCallback {
 public:
  explicit ParseCallback(const base::android::JavaParamRef<jobject>& jcallback)
      : jcallback_(jcallback) {}

  ~ParseCallback() {}

  void OnManifestParseSuccess(
      std::vector<mojom::PaymentManifestSectionPtr> manifest) {
    DCHECK(!manifest.empty());

    JNIEnv* env = base::android::AttachCurrentThread();
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

  void OnManifestParseFailure() {
    // Can trigger synchronous deletion of PaymentManifestParserAndroid.
    Java_ManifestParseCallback_onManifestParseFailure(
        base::android::AttachCurrentThread(), jcallback_);
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> jcallback_;

  DISALLOW_COPY_AND_ASSIGN(ParseCallback);
};

PaymentManifestParserAndroid::PaymentManifestParserAndroid() {}

PaymentManifestParserAndroid::~PaymentManifestParserAndroid() {}

void PaymentManifestParserAndroid::StartUtilityProcess(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  mojo_client_ = base::MakeUnique<
      content::UtilityProcessMojoClient<mojom::PaymentManifestParser>>(
      l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_MANIFEST_PARSER_NAME));
  mojo_client_->set_error_callback(
      base::Bind(&PaymentManifestParserAndroid::OnUtilityProcessStopped,
                 base::Unretained(this)));
  mojo_client_->Start();
}

void PaymentManifestParserAndroid::ParsePaymentManifest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jcontent,
    const base::android::JavaParamRef<jobject>& jcallback) {
  std::unique_ptr<ParseCallback> pending_callback =
      base::MakeUnique<ParseCallback>(jcallback);

  if (!mojo_client_) {
    pending_callback->OnManifestParseFailure();
    return;
  }

  ParseCallback* callback_identifier = pending_callback.get();
  pending_callbacks_.push_back(std::move(pending_callback));
  DCHECK_GE(10U, pending_callbacks_.size());

  mojo_client_->service()->Parse(
      base::android::ConvertJavaStringToUTF8(env, jcontent),
      base::Bind(&PaymentManifestParserAndroid::OnParse, base::Unretained(this),
                 callback_identifier));
}

void PaymentManifestParserAndroid::StopUtilityProcess(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  delete this;
}

void PaymentManifestParserAndroid::OnParse(
    ParseCallback* callback_identifier,
    std::vector<mojom::PaymentManifestSectionPtr> manifest) {
  // At most 10 manifests to parse, so iterating a vector is not too slow.
  for (auto it = pending_callbacks_.begin(); it != pending_callbacks_.end();
       ++it) {
    if (it->get() == callback_identifier) {
      std::unique_ptr<ParseCallback> pending_callback = std::move(*it);
      pending_callbacks_.erase(it);

      // Can trigger synchronous deletion of this object, so can't access any of
      // the member variables after this block.
      if (manifest.empty())
        pending_callback->OnManifestParseFailure();
      else
        pending_callback->OnManifestParseSuccess(std::move(manifest));
      return;
    }
  }

  // If unable to find the pending callback, then something went wrong in the
  // utility process. Stop the utility process and notify all callbacks.
  OnUtilityProcessStopped();
}

void PaymentManifestParserAndroid::OnUtilityProcessStopped() {
  mojo_client_.reset();
  std::vector<std::unique_ptr<ParseCallback>> callbacks =
      std::move(pending_callbacks_);
  for (const auto& callback : callbacks) {
    // Can trigger synchronous deletion of this object, so can't access any of
    // the member variables after this line.
    callback->OnManifestParseFailure();
  }
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
