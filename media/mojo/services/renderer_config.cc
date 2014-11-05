// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/renderer_config.h"

namespace media {

namespace internal {
extern scoped_ptr<PlatformRendererConfig> CreatePlatformRendererConfig();
}  // namespace internal

static base::LazyInstance<RendererConfig>::Leaky g_platform_config =
    LAZY_INSTANCE_INITIALIZER;

// static
RendererConfig* RendererConfig::Get() {
  return g_platform_config.Pointer();
}

ScopedVector<AudioDecoder> RendererConfig::GetAudioDecoders(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const LogCB& media_log_cb) {
  return renderer_config_->GetAudioDecoders(media_task_runner, media_log_cb);
}

ScopedVector<VideoDecoder> RendererConfig::GetVideoDecoders(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const LogCB& media_log_cb) {
  return renderer_config_->GetVideoDecoders(media_task_runner, media_log_cb);
}

scoped_refptr<AudioRendererSink> RendererConfig::GetAudioRendererSink() {
  return renderer_config_->GetAudioRendererSink();
}

const AudioHardwareConfig& RendererConfig::GetAudioHardwareConfig() {
  return renderer_config_->GetAudioHardwareConfig();
}

RendererConfig::RendererConfig()
    : renderer_config_(internal::CreatePlatformRendererConfig().Pass()) {
}

RendererConfig::~RendererConfig() {
}

}  // namespace media
