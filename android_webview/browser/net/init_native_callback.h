// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NET_INIT_NATIVE_CALLBACK_H_
#define ANDROID_WEBVIEW_BROWSER_NET_INIT_NATIVE_CALLBACK_H_

#include "base/memory/scoped_ptr.h"

namespace net {
class CookieMonster;
class URLRequestJobFactory;
}  // namespace net

namespace android_webview {
class AwURLRequestJobFactory;

// This is called on the IO thread when the CookieMonster has been created.
// Note that the UI thread is blocked during this call.
void DidCreateCookieMonster(net::CookieMonster* cookie_monster);

// Called lazily when the job factory is being constructed; allows android
// webview specific request factories to be added to the chain.
scoped_ptr<net::URLRequestJobFactory> CreateAndroidJobFactory(
    scoped_ptr<AwURLRequestJobFactory> job_factory);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NET_INIT_NATIVE_CALLBACK_H_
