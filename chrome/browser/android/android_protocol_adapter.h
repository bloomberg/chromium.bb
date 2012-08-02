// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_ANDROID_PROTOCOL_ADAPTER_H_
#define CHROME_BROWSER_ANDROID_ANDROID_PROTOCOL_ADAPTER_H_

#include <jni.h>

#include "net/url_request/url_request.h"

class AndroidProtocolAdapter {
 public:
  static net::URLRequest::ProtocolFactory Factory;

  // Register the protocol factories for all supported Android protocol
  // schemes.
  static bool RegisterProtocols(JNIEnv* env);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AndroidProtocolAdapter);
};

#endif  // CHROME_BROWSER_ANDROID_ANDROID_PROTOCOL_ADAPTER_H_
