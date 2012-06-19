// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_ANDROID_USER_AGENT_H_
#define CONTENT_APP_ANDROID_USER_AGENT_H_
#pragma once

#include <jni.h>
#include <string>

namespace content {

// Returns the OS information component required by user agent composition.
std::string GetUserAgentOSInfo();

// Register JNI method.
bool RegisterUserAgent(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_APP_ANDROID_USER_AGENT_H_
