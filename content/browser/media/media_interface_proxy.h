// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_PROXY_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_PROXY_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/mojo/features.h"
#include "media/mojo/interfaces/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"

namespace media {
class MediaInterfaceProvider;
}

namespace content {

class RenderFrameHost;

// This implements the media::mojom::InterfaceFactory interface for a
// RenderFrameHostImpl. Upon InterfaceFactory calls, it will
// figure out where to forward to the interface requests. For example,
// - When |enable_standalone_cdm_service| is true, forward CDM request to a
//   standalone CDM service rather than the general media service.
// - Forward CDM requests to different CDM service instances based on library
//   CDM types.
class MediaInterfaceProxy : public media::mojom::InterfaceFactory {
 public:
  // Constructs MediaInterfaceProxy and bind |this| to the |request|. When
  // connection error happens on the client interface, |error_handler| will be
  // called, which could destroy |this|.
  MediaInterfaceProxy(RenderFrameHost* render_frame_host,
                      media::mojom::InterfaceFactoryRequest request,
                      const base::Closure& error_handler);
  ~MediaInterfaceProxy() final;

  // media::mojom::InterfaceFactory implementation.
  void CreateAudioDecoder(media::mojom::AudioDecoderRequest request) final;
  void CreateVideoDecoder(media::mojom::VideoDecoderRequest request) final;
  void CreateRenderer(const std::string& audio_device_id,
                      media::mojom::RendererRequest request) final;
  void CreateCdm(const std::string& key_system,
                 media::mojom::ContentDecryptionModuleRequest request) final;

 private:
  using InterfaceFactoryPtr = media::mojom::InterfaceFactoryPtr;

  // Gets services provided by the browser (at RenderFrameHost level) to the
  // mojo media (or CDM) service running remotely.
  service_manager::mojom::InterfaceProviderPtr GetFrameServices();

  // Gets the MediaService |interface_factory_ptr_|. Returns null if unexpected
  // error happened.
  InterfaceFactory* GetMediaInterfaceFactory();

  void ConnectToMediaService();

  // Callback for connection error from |interface_factory_ptr_|.
  void OnMediaServiceConnectionError();

#if BUILDFLAG(ENABLE_STANDALONE_CDM_SERVICE)
  // Gets a CDM InterfaceFactory pointer for |key_system|. Returns null if
  // unexpected error happened.
  InterfaceFactory* GetCdmInterfaceFactory(const std::string& key_system);

  // Connects to the CDM service associated with |key_system|, adds the new
  // InterfaceFactoryPtr to the |cdm_interface_factory_map_|, and returns the
  // newly created InterfaceFactory pointer. Returns nullptr if unexpected error
  // happened.
  InterfaceFactory* ConnectToCdmService(const std::string& cdm_guid,
                                        const base::FilePath& cdm_path);

  // Callback for connection error from the InterfaceFactoryPtr in the
  // |cdm_interface_factory_map_| associated with |cdm_guid|.
  void OnCdmServiceConnectionError(const std::string& cdm_guid);
#endif  // BUILDFLAG(ENABLE_STANDALONE_CDM_SERVICE)

  // Safe to hold a raw pointer since |this| is owned by RenderFrameHostImpl.
  RenderFrameHost* const render_frame_host_;

  // Binding for incoming InterfaceFactoryRequest from the the RenderFrameImpl.
  mojo::Binding<InterfaceFactory> binding_;

  // TODO(xhwang): Replace InterfaceProvider with a dedicated host interface.
  // See http://crbug.com/660573
  std::vector<std::unique_ptr<media::MediaInterfaceProvider>> media_registries_;

  // InterfacePtr to the remote InterfaceFactory implementation
  // in the service named kMediaServiceName hosted in the process specified by
  // the "mojo_media_host" gn argument. Available options are browser, GPU and
  // utility processes.
  InterfaceFactoryPtr interface_factory_ptr_;

#if BUILDFLAG(ENABLE_STANDALONE_CDM_SERVICE)
  // CDM GUID to CDM InterfaceFactoryPtr mapping, where the InterfaceFactory
  // instances live in the standalone kCdmServiceName service instances.
  std::map<std::string, InterfaceFactoryPtr> cdm_interface_factory_map_;
#endif  // BUILDFLAG(ENABLE_STANDALONE_CDM_SERVICE)

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MediaInterfaceProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_PROXY_H_
