// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/renderer_preferences.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/win/WebFontRendering.h"

using blink::WebFontRendering;

namespace content {

void RenderViewImpl::UpdateFontRenderingFromRendererPrefs() {
  // TODO(ananta)
  // Add code to update the Webkit font cache with the system font metrics
  // information.
}

}  // namespace content
