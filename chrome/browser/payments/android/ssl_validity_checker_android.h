// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_ANDROID_SSL_VALIDITY_CHECKER_ANDROID_H_
#define CHROME_BROWSER_PAYMENTS_ANDROID_SSL_VALIDITY_CHECKER_ANDROID_H_

#include <jni.h>

namespace payments {

bool RegisterSslValidityChecker(JNIEnv* env);

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_ANDROID_SSL_VALIDITY_CHECKER_ANDROID_H_
