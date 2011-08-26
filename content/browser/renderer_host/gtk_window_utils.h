// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_GTK_WINDOW_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_GTK_WINDOW_UTILS_H_
#pragma once

typedef struct _GdkDrawable GdkWindow;

namespace WebKit {
struct WebScreenInfo;
}

namespace content {

void GetScreenInfoFromNativeWindow(
    GdkWindow* gdk_window, WebKit::WebScreenInfo* results);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_GTK_WINDOW_UTILS_H_
