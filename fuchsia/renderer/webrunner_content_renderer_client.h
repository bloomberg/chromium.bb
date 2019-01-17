// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RENDERER_WEBRUNNER_CONTENT_RENDERER_CLIENT_H_
#define FUCHSIA_RENDERER_WEBRUNNER_CONTENT_RENDERER_CLIENT_H_

#include "base/macros.h"
#include "content/public/renderer/content_renderer_client.h"

namespace webrunner {

class WebRunnerContentRendererClient : public content::ContentRendererClient {
 public:
  WebRunnerContentRendererClient();
  ~WebRunnerContentRendererClient() override;

  // content::ContentRendererClient overrides.
  void RenderFrameCreated(content::RenderFrame* render_frame) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRunnerContentRendererClient);
};

}  // namespace webrunner

#endif  // FUCHSIA_RENDERER_WEBRUNNER_CONTENT_RENDERER_CLIENT_H_
