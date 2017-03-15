// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_MANIFEST_PARSER_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_MANIFEST_PARSER_ANDROID_H_

#include <jni.h>

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/payments/content/android/payment_manifest_parser.mojom.h"

namespace content {
template <class MojoInterface>
class UtilityProcessMojoClient;
}

namespace payments {

// Host of the utility process that parses manifest contents.
class PaymentManifestParserAndroid {
 public:
  PaymentManifestParserAndroid();
  ~PaymentManifestParserAndroid();

  void StartUtilityProcess(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jcaller);

  void ParsePaymentManifest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jcontent,
      const base::android::JavaParamRef<jobject>& jcallback);

  // Deletes this object.
  void StopUtilityProcess(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jcaller);

 private:
  class ParseCallback;

  // The |callback_identifier| parameter is a pointer to one of the owned
  // elements in the |pending_callbacks_| list.
  void OnParse(ParseCallback* callback_identifier,
               std::vector<mojom::PaymentManifestSectionPtr> manifest);

  void OnUtilityProcessStopped();

  std::unique_ptr<
      content::UtilityProcessMojoClient<mojom::PaymentManifestParser>>
      mojo_client_;
  std::vector<std::unique_ptr<ParseCallback>> pending_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PaymentManifestParserAndroid);
};

bool RegisterPaymentManifestParser(JNIEnv* env);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_MANIFEST_PARSER_ANDROID_H_
