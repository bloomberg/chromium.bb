// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_

#include <memory>

#include "media/base/audio_decoder.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/renderer_factory.h"
#include "media/base/video_renderer_sink.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace shell {
namespace mojom {
class InterfaceProvider;
}
}

namespace media {

class MojoMediaClient {
 public:
  virtual ~MojoMediaClient();

  // Called exactly once before any other method.
  virtual void Initialize();

  virtual std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // TODO(xhwang): Consider creating Renderer and CDM directly in the client
  // instead of creating factories. See http://crbug.com/586211

  // Returns the RendererFactory to be used by MojoRendererService.
  virtual std::unique_ptr<RendererFactory> CreateRendererFactory(
      const scoped_refptr<MediaLog>& media_log);

  // The output sink used for rendering audio or video respectively. They will
  // be used in the CreateRenderer() call on the RendererFactory returned by
  // CreateRendererFactory(). May be null if the RendererFactory doesn't need an
  // audio or video sink. If not null, the sink must be owned by the client.
  virtual AudioRendererSink* CreateAudioRendererSink();
  virtual VideoRendererSink* CreateVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // Returns the CdmFactory to be used by MojoCdmService.
  virtual std::unique_ptr<CdmFactory> CreateCdmFactory(
      shell::mojom::InterfaceProvider* interface_provider);

 protected:
  MojoMediaClient();
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
