// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_COOKIE_MANAGER_H_
#define ANDROID_WEBVIEW_NATIVE_COOKIE_MANAGER_H_

#include <jni.h>

namespace net {
class CookieMonster;
}  // namespace net

namespace android_webview {
class AwURLRequestJobFactory;

void SetCookieMonsterOnNetworkStackInit(net::CookieMonster* cookie_monster);

bool RegisterCookieManager(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_COOKIE_MANAGER_H_
