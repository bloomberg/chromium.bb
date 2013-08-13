// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webmediaplayer_params.h"

#include "base/message_loop/message_loop_proxy.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/media_log.h"
#include "media/filters/gpu_video_accelerator_factories.h"

namespace content {

WebMediaPlayerParams::WebMediaPlayerParams(
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
    const base::Callback<void(const base::Closure&)>& defer_load_cb,
    const scoped_refptr<media::AudioRendererSink>& audio_renderer_sink,
    const scoped_refptr<media::GpuVideoAcceleratorFactories>& gpu_factories,
    const scoped_refptr<media::MediaLog>& media_log)
    : message_loop_proxy_(message_loop_proxy),
      defer_load_cb_(defer_load_cb),
      audio_renderer_sink_(audio_renderer_sink),
      gpu_factories_(gpu_factories),
      media_log_(media_log) {
  DCHECK(media_log_.get());
}

WebMediaPlayerParams::~WebMediaPlayerParams() {}

}  // namespace content
