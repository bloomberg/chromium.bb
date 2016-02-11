// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_

#include "media/base/audio_renderer_sink.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/renderer_factory.h"
#include "media/base/video_renderer_sink.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace mojo {
namespace shell {
namespace mojom {
class InterfaceProvider;
}
}
}

namespace media {

class MEDIA_EXPORT MojoMediaClient {
 public:
  virtual ~MojoMediaClient();

  // Called exactly once before any other method.
  virtual void Initialize();
  // Returns the RendererFactory to be used by MojoRendererService. If returns
  // null, a RendererImpl will be used with audio/video decoders provided in
  // CreateAudioDecoders() and CreateVideoDecoders().
  virtual scoped_ptr<RendererFactory> CreateRendererFactory(
      const scoped_refptr<MediaLog>& media_log);
  // The output sink used for rendering audio or video respectively. These
  // sinks must be owned by the client.
  virtual AudioRendererSink* CreateAudioRendererSink();
  virtual VideoRendererSink* CreateVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // Returns the CdmFactory to be used by MojoCdmService.
  virtual scoped_ptr<CdmFactory> CreateCdmFactory(
      mojo::shell::mojom::InterfaceProvider* service_provider);

 protected:
  MojoMediaClient();
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
