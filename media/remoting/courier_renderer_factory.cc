// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/courier_renderer_factory.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "build/buildflag.h"
#include "media/base/overlay_info.h"
#include "media/media_features.h"

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
#include "media/remoting/courier_renderer.h"  // nogncheck
#endif

namespace media {
namespace remoting {

CourierRendererFactory::CourierRendererFactory(
    std::unique_ptr<RendererController> controller)
    : controller_(std::move(controller)) {}

CourierRendererFactory::~CourierRendererFactory() {}

std::unique_ptr<Renderer> CourierRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    AudioRendererSink* audio_renderer_sink,
    VideoRendererSink* video_renderer_sink,
    const RequestOverlayInfoCB& request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  DCHECK(IsRemotingActive());
#if !defined(ENABLE_MEDIA_REMOTING_RPC)
  return nullptr;
#else
  return base::MakeUnique<CourierRenderer>(
      media_task_runner, controller_->GetWeakPtr(), video_renderer_sink);
#endif
}

bool CourierRendererFactory::IsRemotingActive() {
  return controller_ && controller_->remote_rendering_started();
}

}  // namespace remoting
}  // namespace media
