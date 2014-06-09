// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_render_frame_ext.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

namespace android_webview {

AwRenderFrameExt::AwRenderFrameExt(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
}

AwRenderFrameExt::~AwRenderFrameExt() {
}

void AwRenderFrameExt::DidCommitProvisionalLoad(bool is_new_navigation) {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  content::DocumentState* document_state =
      content::DocumentState::FromDataSource(frame->dataSource());
  if (document_state->can_load_local_resources()) {
    blink::WebSecurityOrigin origin = frame->document().securityOrigin();
    origin.grantLoadLocalResources();
  }
}

}


