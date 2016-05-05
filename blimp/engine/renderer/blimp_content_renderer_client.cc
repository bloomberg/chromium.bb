// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/renderer/blimp_content_renderer_client.h"

#include "blimp/engine/mojo/blob_channel.mojom.h"
#include "blimp/engine/renderer/engine_image_serialization_processor.h"
#include "components/web_cache/renderer/web_cache_render_thread_observer.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_thread.h"

namespace blimp {
namespace engine {
namespace {

mojom::BlobChannelPtr GetConnectedBlobChannel() {
  mojom::BlobChannelPtr blob_channel_ptr;
  content::RenderThread::Get()->GetServiceRegistry()->ConnectToRemoteService(
      mojo::GetProxy(&blob_channel_ptr));
  DCHECK(blob_channel_ptr);
  return blob_channel_ptr;
}

}  // namespace

BlimpContentRendererClient::BlimpContentRendererClient() {}

BlimpContentRendererClient::~BlimpContentRendererClient() {}

void BlimpContentRendererClient::RenderThreadStarted() {
  web_cache_observer_.reset(new web_cache::WebCacheRenderThreadObserver());
  image_serialization_processor_.reset(
      new EngineImageSerializationProcessor(GetConnectedBlobChannel()));
}

cc::ImageSerializationProcessor*
BlimpContentRendererClient::GetImageSerializationProcessor() {
  return image_serialization_processor_.get();
}

}  // namespace engine
}  // namespace blimp
