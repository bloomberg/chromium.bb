// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/url_constants.h"

namespace android_webview {

// The content: scheme is used in Android for interacting with content
// provides.
// See http://developer.android.com/reference/android/content/ContentUris.html
const char kContentScheme[] = "content";

// These are special paths used with the file: scheme to access application
// assets and resources.
// See http://developer.android.com/reference/android/webkit/WebSettings.html
const char kAndroidAssetPath[] = "/android_asset/";
const char kAndroidResourcePath[] = "/android_res/";

// This scheme is used to display a default HTML5 video poster.
const char kAndroidWebViewVideoPosterScheme[] = "android-webview-video-poster";

}  // namespace android_webview
