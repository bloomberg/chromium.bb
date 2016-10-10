// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PAGE_INFO_CERTIFICATE_VIEWER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_PAGE_INFO_CERTIFICATE_VIEWER_ANDROID_H_

#include "chrome/browser/certificate_viewer.h"

#include <jni.h>

bool RegisterCertificateViewer(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_PAGE_INFO_CERTIFICATE_VIEWER_ANDROID_H_
