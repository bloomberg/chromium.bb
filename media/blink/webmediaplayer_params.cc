// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_params.h"

#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "media/base/audio_renderer_sink.h"

namespace media {

WebMediaPlayerParams::WebMediaPlayerParams(
    std::unique_ptr<MediaLog> media_log,
    const DeferLoadCB& defer_load_cb,
    const scoped_refptr<SwitchableAudioRendererSink>& audio_renderer_sink,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    const Context3DCB& context_3d_cb,
    const AdjustAllocatedMemoryCB& adjust_allocated_memory_cb,
    blink::WebContentDecryptionModule* initial_cdm,
    SurfaceManager* surface_manager,
    const RequestRoutingTokenCallback request_routing_token_cb,
    base::WeakPtr<MediaObserver> media_observer,
    base::TimeDelta max_keyframe_distance_to_disable_background_video,
    base::TimeDelta max_keyframe_distance_to_disable_background_video_mse,
    bool enable_instant_source_buffer_gc,
    bool embedded_media_experience_enabled,
    mojom::WatchTimeRecorderProvider* provider)
    : defer_load_cb_(defer_load_cb),
      audio_renderer_sink_(audio_renderer_sink),
      media_log_(std::move(media_log)),
      media_task_runner_(media_task_runner),
      worker_task_runner_(worker_task_runner),
      compositor_task_runner_(compositor_task_runner),
      context_3d_cb_(context_3d_cb),
      adjust_allocated_memory_cb_(adjust_allocated_memory_cb),
      initial_cdm_(initial_cdm),
      surface_manager_(surface_manager),
      request_routing_token_cb_(std::move(request_routing_token_cb)),
      media_observer_(media_observer),
      max_keyframe_distance_to_disable_background_video_(
          max_keyframe_distance_to_disable_background_video),
      max_keyframe_distance_to_disable_background_video_mse_(
          max_keyframe_distance_to_disable_background_video_mse),
      enable_instant_source_buffer_gc_(enable_instant_source_buffer_gc),
      embedded_media_experience_enabled_(embedded_media_experience_enabled),
      watch_time_recorder_provider_(provider) {}

WebMediaPlayerParams::~WebMediaPlayerParams() {}

}  // namespace media
