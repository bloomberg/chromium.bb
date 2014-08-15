// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHELL_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
#define CHROMECAST_SHELL_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_

#include "content/public/renderer/content_renderer_client.h"

namespace chromecast {
namespace shell {

class CastContentRendererClient : public content::ContentRendererClient {
 public:
  CastContentRendererClient() {}
  virtual ~CastContentRendererClient() {}

  // ContentRendererClient implementation:
  virtual void RenderThreadStarted() OVERRIDE;
  virtual void RenderViewCreated(content::RenderView* render_view) OVERRIDE;
  virtual void AddKeySystems(
      std::vector<content::KeySystemInfo>* key_systems) OVERRIDE;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_SHELL_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
