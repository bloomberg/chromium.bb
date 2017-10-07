// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "media/base/overlay_info.h"
#include "media/media_features.h"
#include "media/mojo/interfaces/video_decoder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gpu {
struct SyncToken;
}

namespace service_manager {
class Connector;
class ServiceContextRefFactory;
namespace mojom {
class InterfaceProvider;
}
}  // namespace service_manager

namespace media {

class AudioDecoder;
class AudioRendererSink;
class CdmFactory;
class MediaLog;
class RendererFactory;
class VideoDecoder;
class VideoFrame;
class VideoRendererSink;
struct CdmHostFilePath;

class MEDIA_MOJO_EXPORT MojoMediaClient {
 public:
  // Similar to VideoFrame::ReleaseMailboxCB for now.
  using ReleaseMailboxCB = base::OnceCallback<void(const gpu::SyncToken&)>;

  using OutputWithReleaseMailboxCB =
      base::Callback<void(ReleaseMailboxCB, const scoped_refptr<VideoFrame>&)>;

  // Called before the host application is scheduled to quit.
  // The application message loop is still valid at this point, so all clean
  // up tasks requiring the message loop must be completed before returning.
  virtual ~MojoMediaClient();

  // Called exactly once before any other method. |connector| can be used by
  // |this| to connect to other services. It is guaranteed to outlive |this|.
  virtual void Initialize(
      service_manager::Connector* connector,
      service_manager::ServiceContextRefFactory* context_ref_factory);

  virtual std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // TODO(sandersd): |output_cb| should not be required.
  // See https://crbug.com/733828.
  virtual std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log,
      mojom::CommandBufferIdPtr command_buffer_id,
      OutputWithReleaseMailboxCB output_cb,
      RequestOverlayInfoCB request_overlay_info_cb);

  // Returns the output sink used for rendering audio on |audio_device_id|.
  // May be null if the RendererFactory doesn't need an audio sink.
  virtual scoped_refptr<AudioRendererSink> CreateAudioRendererSink(
      const std::string& audio_device_id);

  // Returns the output sink used for rendering video.
  // May be null if the RendererFactory doesn't need a video sink.
  virtual std::unique_ptr<VideoRendererSink> CreateVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // Returns the RendererFactory to be used by MojoRendererService.
  virtual std::unique_ptr<RendererFactory> CreateRendererFactory(
      MediaLog* media_log);

  // Returns the CdmFactory to be used by MojoCdmService. |host_interfaces| can
  // be used to request interfaces provided remotely by the host. It may be a
  // nullptr if the host chose not to bind the InterfacePtr.
  virtual std::unique_ptr<CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* host_interfaces);

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  // Gets a list of CDM host file paths and put them in |cdm_host_file_paths|.
  virtual void AddCdmHostFilePaths(
      std::vector<CdmHostFilePath>* cdm_host_file_paths);
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

 protected:
  MojoMediaClient();
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
