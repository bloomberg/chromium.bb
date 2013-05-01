// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_IN_PROCESS_RENDERER_CLIENT_
#define ANDROID_WEBVIEW_COMMON_IN_PROCESS_RENDERER_CLIENT_

#include "android_webview/renderer/aw_content_renderer_client.h"

namespace android_webview {

// Specialization of the Renderer client used in single process mode, to
// route various factory-like methods to browser/ side code (in general,
// this is only needed / allowed in order to meet synchronous rendering
// requirements).
class InProcessRendererClient : public AwContentRendererClient {
 public:
  virtual base::MessageLoop* OverrideCompositorMessageLoop() const OVERRIDE;
  virtual void DidCreateSynchronousCompositor(
      int render_view_id,
      content::SynchronousCompositor* compositor) OVERRIDE;
  virtual bool ShouldCreateCompositorInputHandler() const OVERRIDE;
};

}  // android_webview

#endif  // ANDROID_WEBVIEW_COMMON_IN_PROCESS_RENDERER_CLIENT_
