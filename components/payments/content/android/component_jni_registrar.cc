// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "components/payments/content/android/currency_formatter_android.h"
#include "components/payments/content/android/payment_details_validation_android.h"
#include "components/payments/content/android/payment_manifest_downloader_android.h"
#include "components/payments/content/android/payment_manifest_parser_android.h"

namespace payments {
namespace android {

static base::android::RegistrationMethod kPaymentsRegisteredMethods[] = {
    {"CurrencyFormatter", CurrencyFormatterAndroid::Register},
    {"PaymentManifestDownloader", RegisterPaymentManifestDownloader},
    {"PaymentManifestParser", RegisterPaymentManifestParser},
    {"PaymentValidator", RegisterPaymentValidator},
};

bool RegisterPayments(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kPaymentsRegisteredMethods, arraysize(kPaymentsRegisteredMethods));
}

}  // namespace android
}  // namespace payments