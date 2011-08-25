// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/mac/WebScreenInfoFactory.h"

// TODO(jam): move this to render_widget_host_view_mac.mm when it moves to
// content.
#if defined(OS_MACOSX)
// static
void RenderWidgetHostView::GetDefaultScreenInfo(
    WebKit::WebScreenInfo* results) {
  *results = WebKit::WebScreenInfoFactory::screenInfo(NULL);
}
#endif

RenderWidgetHostView::~RenderWidgetHostView() {}

void RenderWidgetHostView::SetBackground(const SkBitmap& background) {
  background_ = background;
}
