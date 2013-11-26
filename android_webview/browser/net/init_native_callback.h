// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NET_INIT_NATIVE_CALLBACK_H_
#define ANDROID_WEBVIEW_BROWSER_NET_INIT_NATIVE_CALLBACK_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {
class CookieStore;
}  // namespace net

namespace android_webview {
class AwBrowserContext;

// Called when the CookieMonster needs to be created.
scoped_refptr<net::CookieStore> CreateCookieStore(
    AwBrowserContext* browser_context);

// Called lazily when the job factory is being constructed.
scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
    CreateAndroidAssetFileProtocolHandler();

// Called lazily when the job factory is being constructed.
scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
    CreateAndroidContentProtocolHandler();

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NET_INIT_NATIVE_CALLBACK_H_
