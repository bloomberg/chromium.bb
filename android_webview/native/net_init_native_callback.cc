// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/init_native_callback.h"

#include "android_webview/native/android_protocol_handler.h"
#include "android_webview/native/cookie_manager.h"

namespace android_webview {
class AwURLRequestJobFactory;

void OnNetworkStackInitialized(net::URLRequestContext* context,
                               AwURLRequestJobFactory* job_factory) {
  RegisterAndroidProtocolsOnIOThread(context, job_factory);
  SetCookieMonsterOnNetworkStackInit(context, job_factory);
}

}
