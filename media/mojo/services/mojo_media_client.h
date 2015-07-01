// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_

#include "base/lazy_instance.h"
#include "base/memory/scoped_vector.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/media_log.h"
#include "media/base/video_decoder.h"
#include "media/base/video_renderer_sink.h"

namespace media {

// Interface class which clients will extend to override (at compile time) the
// default configurations for mojo media services.
class PlatformMojoMediaClient {
 public:
  virtual ~PlatformMojoMediaClient() {};

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

  // The output sink used for rendering audio or video respectively.
  virtual scoped_refptr<AudioRendererSink> GetAudioRendererSink() = 0;
  virtual scoped_ptr<VideoRendererSink> GetVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) = 0;

  // The platform's audio hardware configuration.  Note, this must remain
  // constant for the lifetime of the PlatformMojoMediaClient.
  virtual const AudioHardwareConfig& GetAudioHardwareConfig() = 0;
};

class MojoMediaClient {
 public:
  // Returns an instance of the MojoMediaClient object.  Only one instance will
  // exist per process.
  static MojoMediaClient* Get();

  // Copy of the PlatformMojoMediaClient interface.
  ScopedVector<AudioDecoder> GetAudioDecoders(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const LogCB& media_log_cb);
  ScopedVector<VideoDecoder> GetVideoDecoders(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const LogCB& media_log_cb);
  scoped_refptr<AudioRendererSink> GetAudioRendererSink();
  scoped_ptr<VideoRendererSink> GetVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  const AudioHardwareConfig& GetAudioHardwareConfig();

 private:
  friend struct base::DefaultLazyInstanceTraits<MojoMediaClient>;

  MojoMediaClient();
  ~MojoMediaClient();

  scoped_ptr<PlatformMojoMediaClient> mojo_media_client_;

  DISALLOW_COPY_AND_ASSIGN(MojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
