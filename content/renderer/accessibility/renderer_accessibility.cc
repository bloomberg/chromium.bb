// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/renderer_accessibility.h"

#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebDocument;
using blink::WebView;

namespace content {

RendererAccessibility::RendererAccessibility(
    RenderFrameImpl* render_frame)
    : RenderFrameObserver(render_frame),
      render_frame_(render_frame) {
}

RendererAccessibility::~RendererAccessibility() {
}

WebDocument RendererAccessibility::GetMainDocument() {
  if (render_frame_ && render_frame_->GetWebFrame())
    return render_frame_->GetWebFrame()->document();
  return WebDocument();
}

}  // namespace content
