// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/render_widget_host_view.h"

#include "base/logging.h"

// static
void content::RenderWidgetHostViewPort::GetDefaultScreenInfo(
    WebKit::WebScreenInfo* results) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostView, public:

// static
content::RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  NOTIMPLEMENTED();
  return NULL;
}

