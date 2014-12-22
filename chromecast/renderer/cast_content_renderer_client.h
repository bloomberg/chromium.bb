// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
#define CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_

#include <vector>

#include "base/macros.h"
#include "content/public/renderer/content_renderer_client.h"

namespace dns_prefetch {
class PrescientNetworkingDispatcher;
}  // namespace dns_prefetch

namespace chromecast {
namespace shell {

class CastContentRendererClient : public content::ContentRendererClient {
 public:
  CastContentRendererClient();
  ~CastContentRendererClient() override;

  // ContentRendererClient implementation:
  void RenderThreadStarted() override;
  void RenderViewCreated(content::RenderView* render_view) override;
  void AddKeySystems(
      std::vector< ::media::KeySystemInfo>* key_systems) override;
  scoped_ptr<media::RendererFactory> CreateMediaRendererFactory(
      content::RenderFrame* render_frame) override;
  blink::WebPrescientNetworking* GetPrescientNetworking() override;
  void DeferMediaLoad(content::RenderFrame* render_frame,
                      const base::Closure& closure) override;

 private:
  scoped_ptr<dns_prefetch::PrescientNetworkingDispatcher>
      prescient_networking_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(CastContentRendererClient);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
