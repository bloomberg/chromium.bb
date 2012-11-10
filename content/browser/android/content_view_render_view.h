// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIEW_RENDER_VIEW_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIEW_RENDER_VIEW_H_

#include <jni.h>

namespace content {

// Registers the JNI methods for ContentViewRender.
bool RegisterContentViewRenderView(JNIEnv* env);

}

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIEW_RENDER_VIEW_H_
