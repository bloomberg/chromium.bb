// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_RENDERER_CONFIG_H_
#define MEDIA_MOJO_SERVICES_RENDERER_CONFIG_H_

#include "base/lazy_instance.h"
#include "base/memory/scoped_vector.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/media_log.h"

namespace media {

class PlatformRendererConfig {
 public:
  virtual ~PlatformRendererConfig() {};

  virtual ScopedVector<AudioDecoder> GetAudioDecoders(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const LogCB& media_log_cb) = 0;
  virtual scoped_refptr<AudioRendererSink> GetAudioRendererSink() = 0;
  virtual const AudioHardwareConfig& GetAudioHardwareConfig() = 0;
};

class RendererConfig {
 public:
  static RendererConfig* Get();

  ScopedVector<AudioDecoder> GetAudioDecoders(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const LogCB& media_log_cb);
  scoped_refptr<AudioRendererSink> GetAudioRendererSink();
  const AudioHardwareConfig& GetAudioHardwareConfig();

 private:
  friend struct base::DefaultLazyInstanceTraits<RendererConfig>;

  RendererConfig();
  ~RendererConfig();

  scoped_ptr<PlatformRendererConfig> renderer_config_;

  DISALLOW_COPY_AND_ASSIGN(RendererConfig);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_RENDERER_CONFIG_H_
