// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_SUGGESTIONS_EVENT_REPORTER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_NTP_SUGGESTIONS_EVENT_REPORTER_BRIDGE_H_

#include <jni.h>

// The C++ counterpart to SuggestionsEventReporterBridge.java.
//
// It exposes static calls that record metrics and interact with the user
// classifier
bool RegisterSuggestionsEventReporterBridge(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_NTP_SUGGESTIONS_EVENT_REPORTER_BRIDGE_H_
