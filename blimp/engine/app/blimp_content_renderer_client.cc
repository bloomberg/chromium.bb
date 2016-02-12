// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_content_renderer_client.h"

#include "blimp/common/compositor/blimp_image_serialization_processor.h"
#include "components/web_cache/renderer/web_cache_render_process_observer.h"
#include "content/public/renderer/render_thread.h"

namespace blimp {
namespace engine {

BlimpContentRendererClient::BlimpContentRendererClient(
    scoped_ptr<BlimpImageSerializationProcessor> image_serialization_processor)
    : image_serialization_processor_(std::move(image_serialization_processor)) {
}

BlimpContentRendererClient::~BlimpContentRendererClient() {}

void BlimpContentRendererClient::RenderThreadStarted() {
  web_cache_observer_.reset(new web_cache::WebCacheRenderProcessObserver());
  content::RenderThread::Get()->AddObserver(web_cache_observer_.get());
}

cc::ImageSerializationProcessor*
BlimpContentRendererClient::GetImageSerializationProcessor() {
  return image_serialization_processor_.get();
}

}  // namespace engine
}  // namespace blimp
