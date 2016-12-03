// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_PROXY_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_PROXY_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/mojo/interfaces/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/interface_registry.h"

namespace content {

class RenderFrameHost;

// This implements the media::mojom::InterfaceFactory interface for
// RenderFrameHostImpl. Upon media::mojom::InterfaceFactory calls, it will
// figure out where to forward to the interface requests.
class MediaInterfaceProxy : public media::mojom::InterfaceFactory {
 public:
  // Constructs MediaInterfaceProxy and bind |this| to the |request|. When
  // connection error happens on the client interface, |error_handler| should be
  // fired.
  MediaInterfaceProxy(RenderFrameHost* render_frame_host,
                      media::mojom::InterfaceFactoryRequest request,
                      const base::Closure& error_handler);
  ~MediaInterfaceProxy() final;

  // media::mojom::InterfaceFactory implementation.
  void CreateAudioDecoder(media::mojom::AudioDecoderRequest request) final;
  void CreateVideoDecoder(media::mojom::VideoDecoderRequest request) final;
  void CreateRenderer(const std::string& audio_device_id,
                      media::mojom::RendererRequest request) final;
  void CreateCdm(media::mojom::ContentDecryptionModuleRequest request) final;

 private:
  media::mojom::InterfaceFactory* GetMediaInterfaceFactory();

  // Callback for connection error on |interface_factory_ptr_|.
  void OnConnectionError();

  void ConnectToService();

  // Safe to hold a raw pointer since |this| is owned by RenderFrameHostImpl.
  RenderFrameHost* render_frame_host_;

  // TODO(xhwang): Replace InterfaceProvider with a dedicated host interface.
  // See http://crbug.com/660573
  std::vector<std::unique_ptr<service_manager::InterfaceRegistry>>
      media_registries_;

  mojo::Binding<media::mojom::InterfaceFactory> binding_;

  // InterfacePtr to the remote media::mojom::InterfaceFactory implementation.
  media::mojom::InterfaceFactoryPtr interface_factory_ptr_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MediaInterfaceProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_PROXY_H_
