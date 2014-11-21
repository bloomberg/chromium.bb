// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_BRIDGE_TEST_ANDROID_CLIENT_WEB_CLIENT_ANDROID_H_
#define COMPONENTS_DEVTOOLS_BRIDGE_TEST_ANDROID_CLIENT_WEB_CLIENT_ANDROID_H_

#include <jni.h>

#include "components/devtools_bridge/client/web_client.h"

class Profile;

namespace devtools_bridge {
namespace android {

/**
 * Android wrapper over WebClient for Java tests. See WebClientTest.java.
 */
class WebClientAndroid : private WebClient::Delegate {
 public:
  static bool RegisterNatives(JNIEnv* env);

  WebClientAndroid(Profile* profile);
  ~WebClientAndroid();

 private:
  scoped_ptr<WebClient> impl_;
};

}  // namespace android
}  // namespace devtools_bridge

#endif  // COMPONENTS_DEVTOOLS_BRIDGE_TEST_ANDROID_CLIENT_WEB_CLIENT_ANDROID_H_
