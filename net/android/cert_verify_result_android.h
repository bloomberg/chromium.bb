// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_CERT_VERIFY_RESULT_ANDROID_H_
#define NET_ANDROID_CERT_VERIFY_RESULT_ANDROID_H_

#include "base/basictypes.h"
#include "base/platform_file.h"
#include "net/base/net_export.h"

namespace net {

namespace android {

enum CertVerifyResultAndroid {
#define CERT_VERIFY_RESULT_ANDROID(label, value) VERIFY_ ## label = value,
#include "net/android/cert_verify_result_android_list.h"
#undef CERT_VERIFY_RESULT_ANDROID
};

}  // namespace android

}  // namespace net

#endif  // NET_ANDROID_CERT_VERIFY_RESULT_ANDROID_H_
