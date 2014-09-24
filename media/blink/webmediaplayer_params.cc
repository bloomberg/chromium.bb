// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_params.h"

#include "base/single_thread_task_runner.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/media_log.h"
#include "media/filters/gpu_video_accelerator_factories.h"

namespace media {

WebMediaPlayerParams::WebMediaPlayerParams(
    const base::Callback<void(const base::Closure&)>& defer_load_cb,
    const scoped_refptr<AudioRendererSink>& audio_renderer_sink,
    const AudioHardwareConfig& audio_hardware_config,
    const scoped_refptr<MediaLog>& media_log,
    const scoped_refptr<GpuVideoAcceleratorFactories>& gpu_factories,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    const EncryptedMediaPlayerSupportCreateCB&
        encrypted_media_player_support_cb,
    blink::WebContentDecryptionModule* initial_cdm)
    : defer_load_cb_(defer_load_cb),
      audio_renderer_sink_(audio_renderer_sink),
      audio_hardware_config_(audio_hardware_config),
      media_log_(media_log),
      gpu_factories_(gpu_factories),
      media_task_runner_(media_task_runner),
      compositor_task_runner_(compositor_task_runner),
      encrypted_media_player_support_cb_(encrypted_media_player_support_cb),
      initial_cdm_(initial_cdm) {
}

WebMediaPlayerParams::~WebMediaPlayerParams() {}

scoped_ptr<EncryptedMediaPlayerSupport>
WebMediaPlayerParams::CreateEncryptedMediaPlayerSupport(
    blink::WebMediaPlayerClient* client) const {
  scoped_ptr<EncryptedMediaPlayerSupport> encrypted_media_support =
      encrypted_media_player_support_cb_.Run(client);
  if (encrypted_media_support)
    encrypted_media_support->SetInitialContentDecryptionModule(initial_cdm_);
  return encrypted_media_support.Pass();
}

}  // namespace media
