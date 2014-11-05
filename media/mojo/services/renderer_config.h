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
#include "media/base/video_decoder.h"

namespace media {

// Interface class which clients will extend to override (at compile time) the
// default audio or video rendering configurations for MojoRendererService.
class PlatformRendererConfig {
 public:
  virtual ~PlatformRendererConfig() {};

  // The list of audio or video decoders for use with the AudioRenderer or
  // VideoRenderer respectively.  Ownership of the decoders is passed to the
  // caller.  The methods on each decoder will only be called on
  // |media_task_runner|.  |media_log_cb| should be used to log errors or
  // important status information.
  virtual ScopedVector<AudioDecoder> GetAudioDecoders(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const LogCB& media_log_cb) = 0;
  virtual ScopedVector<VideoDecoder> GetVideoDecoders(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const LogCB& media_log_cb) = 0;

  // The audio output sink used for rendering audio.
  virtual scoped_refptr<AudioRendererSink> GetAudioRendererSink() = 0;

  // The platform's audio hardware configuration.  Note, this must remain
  // constant for the lifetime of the PlatformRendererConfig.
  virtual const AudioHardwareConfig& GetAudioHardwareConfig() = 0;

};

class RendererConfig {
 public:
  // Returns an instance of the RenderConfig object.  Only one instance will
  // exist per process.
  static RendererConfig* Get();

  // Copy of the PlatformRendererConfig interface.
  ScopedVector<AudioDecoder> GetAudioDecoders(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const LogCB& media_log_cb);
  ScopedVector<VideoDecoder> GetVideoDecoders(
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
