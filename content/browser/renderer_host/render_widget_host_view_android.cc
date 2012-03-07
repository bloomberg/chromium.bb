// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/render_widget_host_view.h"

#include "base/logging.h"
#include "content/port/browser/render_widget_host_view_port.h"

// static
void content::RenderWidgetHostViewPort::GetDefaultScreenInfo(
    WebKit::WebScreenInfo* results) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostView, public:

// static
content::RenderWidgetHostView*
content::RenderWidgetHostView::CreateViewForWidget(
    content::RenderWidgetHost* widget) {
  NOTIMPLEMENTED();
  return NULL;
}
