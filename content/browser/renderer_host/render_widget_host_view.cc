// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view.h"

RenderWidgetHostView::~RenderWidgetHostView() {}

void RenderWidgetHostView::SetBackground(const SkBitmap& background) {
  background_ = background;
}
