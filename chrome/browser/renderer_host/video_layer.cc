// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/video_layer.h"

VideoLayer::VideoLayer(RenderWidgetHost* widget, const gfx::Size& size)
    : render_widget_host_(widget),
      size_(size) {
}

VideoLayer::~VideoLayer() {
}
