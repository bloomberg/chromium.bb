// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/android/chrome_payments_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "chrome/browser/payments/android/journey_logger_android.h"
#include "chrome/browser/payments/android/payment_manifest_web_data_service_android.h"
#include "chrome/browser/payments/android/ssl_validity_checker_android.h"

namespace payments {
namespace android {

static base::android::RegistrationMethod kChromePaymentsRegisteredMethods[] = {
    {"JourneyLogger", JourneyLoggerAndroid::Register},
    {"SslValidityChecker", RegisterSslValidityChecker},
    {"PaymentManifestWebDataService",
     PaymentManifestWebDataServiceAndroid::Register},
};

bool RegisterChromePayments(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kChromePaymentsRegisteredMethods,
      arraysize(kChromePaymentsRegisteredMethods));
}

}  // namespace android
}  // namespace payments
