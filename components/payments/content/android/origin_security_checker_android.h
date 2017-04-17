// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_ORIGIN_SECURITY_CHECKER_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_ORIGIN_SECURITY_CHECKER_ANDROID_H_

#include <jni.h>

namespace payments {

bool RegisterOriginSecurityChecker(JNIEnv* env);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_ORIGIN_SECURITY_CHECKER_ANDROID_H_
