// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "media/mojo/services/media_mojo_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace shell {
namespace mojom {
class InterfaceProvider;
}
}

namespace media {

class AudioDecoder;
class CdmFactory;
class MediaLog;
class Renderer;
class VideoDecoder;

class MEDIA_MOJO_EXPORT MojoMediaClient {
 public:
  virtual ~MojoMediaClient();

  // Called exactly once before any other method.
  virtual void Initialize();

  // Called before the host application is scheduled to quit.
  // The application message loop is still valid at this point, so all clean
  // up tasks requiring the message loop must be completed before returning.
  virtual void WillQuit();

  virtual std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  virtual std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // TODO(xhwang): Consider creating CDM directly in the client
  // instead of creating factories. See http://crbug.com/586211

  // Creates and returns a Renderer.
  virtual std::unique_ptr<Renderer> CreateRenderer(
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      scoped_refptr<MediaLog> media_log,
      const std::string& audio_device_id);

  // Returns the CdmFactory to be used by MojoCdmService.
  virtual std::unique_ptr<CdmFactory> CreateCdmFactory(
      shell::mojom::InterfaceProvider* interface_provider);

 protected:
  MojoMediaClient();
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
