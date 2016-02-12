// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_BLIMP_CONTENT_RENDERER_CLIENT_H_
#define BLIMP_ENGINE_APP_BLIMP_CONTENT_RENDERER_CLIENT_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/content_renderer_client.h"

namespace web_cache {
class WebCacheRenderProcessObserver;
}

namespace blimp {
class BlimpImageSerializationProcessor;

namespace engine {

class BlimpContentRendererClient : public content::ContentRendererClient {
 public:
  BlimpContentRendererClient(scoped_ptr<BlimpImageSerializationProcessor>
                                 image_serialization_processor);
  ~BlimpContentRendererClient() override;

  // content::ContentRendererClient implementation.
  void RenderThreadStarted() override;
  cc::ImageSerializationProcessor* GetImageSerializationProcessor() override;

 private:
  // This observer manages the process-global web cache.
  scoped_ptr<web_cache::WebCacheRenderProcessObserver> web_cache_observer_;

  // Provides the functionality to serialize images in SkPicture.
  scoped_ptr<BlimpImageSerializationProcessor> image_serialization_processor_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentRendererClient);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_BLIMP_CONTENT_RENDERER_CLIENT_H_
