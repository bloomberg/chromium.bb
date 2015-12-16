// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_

#include "base/lazy_instance.h"
#include "base/memory/scoped_vector.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/renderer_factory.h"
#include "media/base/video_renderer_sink.h"

namespace mojo {
class ServiceProvider;
}

namespace media {

// Interface class which clients will extend to override (at compile time) the
// default configurations for mojo media services.
class PlatformMojoMediaClient {
 public:
  virtual ~PlatformMojoMediaClient();

  // Returns the RendererFactory to be used by MojoRendererService. If returns
  // null, a RendererImpl will be used with audio/video decoders provided in
  // CreateAudioDecoders() and CreateVideoDecoders().
  virtual scoped_ptr<RendererFactory> CreateRendererFactory(
      const scoped_refptr<MediaLog>& media_log);
  // The output sink used for rendering audio or video respectively.
  virtual scoped_refptr<AudioRendererSink> CreateAudioRendererSink();
  virtual scoped_ptr<VideoRendererSink> CreateVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // Returns the CdmFactory to be used by MojoCdmService.
  virtual scoped_ptr<CdmFactory> CreateCdmFactory(
      mojo::ServiceProvider* service_provider);
};

class MojoMediaClient {
 public:
  // Returns an instance of the MojoMediaClient object.  Only one instance will
  // exist per process.
  static MojoMediaClient* Get();

  // Copy of the PlatformMojoMediaClient interface.
  scoped_ptr<RendererFactory> CreateRendererFactory(
      const scoped_refptr<MediaLog>& media_log);
  scoped_refptr<AudioRendererSink> CreateAudioRendererSink();
  scoped_ptr<VideoRendererSink> CreateVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  scoped_ptr<CdmFactory> CreateCdmFactory(
      mojo::ServiceProvider* service_provider);

 private:
  friend struct base::DefaultLazyInstanceTraits<MojoMediaClient>;

  MojoMediaClient();
  ~MojoMediaClient();

  scoped_ptr<PlatformMojoMediaClient> mojo_media_client_;

  DISALLOW_COPY_AND_ASSIGN(MojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
