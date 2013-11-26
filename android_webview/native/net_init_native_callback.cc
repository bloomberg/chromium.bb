// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/init_native_callback.h"

#include "android_webview/browser/net/aw_url_request_job_factory.h"
#include "android_webview/native/android_protocol_handler.h"
#include "base/logging.h"

namespace android_webview {
class AwURLRequestJobFactory;

scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
CreateAndroidAssetFileProtocolHandler() {
  return CreateAssetFileProtocolHandler();
}

scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
CreateAndroidContentProtocolHandler() {
  return CreateContentSchemeProtocolHandler();
}

}  // namespace android_webview
