// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBMEDIAPLAYER_PARAMS_H_
#define MEDIA_BLINK_WEBMEDIAPLAYER_PARAMS_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/viz/common/gpu/context_provider.h"
#include "media/base/media_log.h"
#include "media/base/media_observer.h"
#include "media/base/media_switches.h"
#include "media/base/routing_token_callback.h"
#include "media/blink/media_blink_export.h"
#include "media/filters/context_3d.h"
#include "media/mojo/interfaces/media_metrics_provider.mojom.h"
#include "third_party/blink/public/platform/web_video_frame_submitter.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace blink {
class WebContentDecryptionModule;
class WebSurfaceLayerBridge;
class WebSurfaceLayerBridgeObserver;
}  // namespace blink

namespace viz {
class SurfaceId;
}

namespace media {

class SwitchableAudioRendererSink;
class SurfaceManager;

// Holds parameters for constructing WebMediaPlayerImpl without having
// to plumb arguments through various abstraction layers.
class MEDIA_BLINK_EXPORT WebMediaPlayerParams {
 public:
  typedef base::Callback<void(const base::Closure&)> DeferLoadCB;
  typedef base::Callback<Context3D()> Context3DCB;

  // Callback to obtain the video SurfaceInfo to trigger Picture-in-Picture
  // mode.
  using PipSurfaceInfoCB =
      base::RepeatingCallback<void(const viz::SurfaceId& surface_id)>;

  // Callback to exit Picture-in-Picture.
  using ExitPipCB = base::RepeatingCallback<void()>;

  // Callback to obtain the media ContextProvider.
  // Requires being called on the media thread.
  // The argument callback is also called on the media thread as a reply.
  using ContextProviderCB =
      base::Callback<void(base::Callback<void(viz::ContextProvider*)>)>;

  // Callback to tell V8 about the amount of memory used by the WebMediaPlayer
  // instance.  The input parameter is the delta in bytes since the last call to
  // AdjustAllocatedMemoryCB and the return value is the total number of bytes
  // used by objects external to V8.  Note: this value includes things that are
  // not the WebMediaPlayer!
  typedef base::Callback<int64_t(int64_t)> AdjustAllocatedMemoryCB;

  // |defer_load_cb|, |audio_renderer_sink|, |compositor_task_runner|, and
  // |context_3d_cb| may be null.
  WebMediaPlayerParams(
      std::unique_ptr<MediaLog> media_log,
      const DeferLoadCB& defer_load_cb,
      const scoped_refptr<SwitchableAudioRendererSink>& audio_renderer_sink,
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>&
          video_frame_compositor_task_runner,
      const AdjustAllocatedMemoryCB& adjust_allocated_memory_cb,
      blink::WebContentDecryptionModule* initial_cdm,
      SurfaceManager* surface_manager,
      RequestRoutingTokenCallback request_routing_token_cb,
      base::WeakPtr<MediaObserver> media_observer,
      base::TimeDelta max_keyframe_distance_to_disable_background_video,
      base::TimeDelta max_keyframe_distance_to_disable_background_video_mse,
      bool enable_instant_source_buffer_gc,
      bool embedded_media_experience_enabled,
      mojom::MediaMetricsProviderPtr metrics_provider,
      base::Callback<std::unique_ptr<blink::WebSurfaceLayerBridge>(
          blink::WebSurfaceLayerBridgeObserver*)> bridge_callback,
      scoped_refptr<viz::ContextProvider> context_provider,
      bool use_surface_layer_for_video,
      const PipSurfaceInfoCB& surface_info_cb,
      const ExitPipCB& exit_pip_cb);

  ~WebMediaPlayerParams();

  DeferLoadCB defer_load_cb() const { return defer_load_cb_; }

  const scoped_refptr<SwitchableAudioRendererSink>& audio_renderer_sink()
      const {
    return audio_renderer_sink_;
  }

  std::unique_ptr<MediaLog> take_media_log() { return std::move(media_log_); }

  mojom::MediaMetricsProviderPtr take_metrics_provider() {
    return std::move(metrics_provider_);
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner() const {
    return media_task_runner_;
  }

  const scoped_refptr<base::TaskRunner> worker_task_runner() const {
    return worker_task_runner_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner()
      const {
    return compositor_task_runner_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>&
  video_frame_compositor_task_runner() const {
    return video_frame_compositor_task_runner_;
  }

  blink::WebContentDecryptionModule* initial_cdm() const {
    return initial_cdm_;
  }

  AdjustAllocatedMemoryCB adjust_allocated_memory_cb() const {
    return adjust_allocated_memory_cb_;
  }

  SurfaceManager* surface_manager() const { return surface_manager_; }

  base::WeakPtr<MediaObserver> media_observer() const {
    return media_observer_;
  }

  base::TimeDelta max_keyframe_distance_to_disable_background_video() const {
    return max_keyframe_distance_to_disable_background_video_;
  }

  base::TimeDelta max_keyframe_distance_to_disable_background_video_mse()
      const {
    return max_keyframe_distance_to_disable_background_video_mse_;
  }

  bool enable_instant_source_buffer_gc() const {
    return enable_instant_source_buffer_gc_;
  }

  bool embedded_media_experience_enabled() const {
    return embedded_media_experience_enabled_;
  }

  RequestRoutingTokenCallback request_routing_token_cb() {
    return request_routing_token_cb_;
  }

  const base::Callback<std::unique_ptr<blink::WebSurfaceLayerBridge>(
      blink::WebSurfaceLayerBridgeObserver*)>& create_bridge_callback() const {
    return create_bridge_callback_;
  }

  scoped_refptr<viz::ContextProvider> context_provider() {
    return context_provider_;
  }

  bool use_surface_layer_for_video() const {
    return use_surface_layer_for_video_;
  }

  const PipSurfaceInfoCB pip_surface_info_cb() const {
    return pip_surface_info_cb_;
  }

  const ExitPipCB exit_pip_cb() const { return exit_pip_cb_; }

 private:
  DeferLoadCB defer_load_cb_;
  scoped_refptr<SwitchableAudioRendererSink> audio_renderer_sink_;
  std::unique_ptr<MediaLog> media_log_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<base::TaskRunner> worker_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner>
      video_frame_compositor_task_runner_;
  AdjustAllocatedMemoryCB adjust_allocated_memory_cb_;

  blink::WebContentDecryptionModule* initial_cdm_;
  SurfaceManager* surface_manager_;
  RequestRoutingTokenCallback request_routing_token_cb_;
  base::WeakPtr<MediaObserver> media_observer_;
  base::TimeDelta max_keyframe_distance_to_disable_background_video_;
  base::TimeDelta max_keyframe_distance_to_disable_background_video_mse_;
  bool enable_instant_source_buffer_gc_;
  const bool embedded_media_experience_enabled_;
  mojom::MediaMetricsProviderPtr metrics_provider_;
  base::Callback<std::unique_ptr<blink::WebSurfaceLayerBridge>(
      blink::WebSurfaceLayerBridgeObserver*)>
      create_bridge_callback_;
  scoped_refptr<viz::ContextProvider> context_provider_;
  bool use_surface_layer_for_video_;
  PipSurfaceInfoCB pip_surface_info_cb_;
  ExitPipCB exit_pip_cb_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerParams);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBMEDIAPLAYER_PARAMS_H_
