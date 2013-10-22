// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_X509_UTIL_H_
#define NET_ANDROID_X509_UTIL_H_

#include <jni.h>

namespace net {

bool RegisterX509Util(JNIEnv* env);

}  // net namespace

#endif  // NET_ANDROID_X509_UTIL_H_
