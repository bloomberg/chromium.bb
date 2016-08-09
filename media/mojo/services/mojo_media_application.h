// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "media/mojo/interfaces/media_service.mojom.h"
#include "media/mojo/interfaces/service_factory.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_context_ref.h"
#include "url/gurl.h"

namespace media {

class MediaLog;
class MojoMediaClient;

class MEDIA_MOJO_EXPORT MojoMediaApplication
    : public NON_EXPORTED_BASE(shell::Service),
      public NON_EXPORTED_BASE(shell::InterfaceFactory<mojom::MediaService>),
      public NON_EXPORTED_BASE(mojom::MediaService) {
 public:
  MojoMediaApplication(std::unique_ptr<MojoMediaClient> mojo_media_client,
                       const base::Closure& quit_closure);
  ~MojoMediaApplication() final;

 private:
  // shell::Service implementation.
  void OnStart(const shell::Identity& identity) final;
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) final;
  bool OnStop() final;

  // shell::InterfaceFactory<mojom::MediaService> implementation.
  void Create(const shell::Identity& remote_identity,
              mojom::MediaServiceRequest request) final;

  // mojom::MediaService implementation.
  void CreateServiceFactory(
      mojom::ServiceFactoryRequest request,
      shell::mojom::InterfaceProviderPtr remote_interfaces) final;

  // Note: Since each instance runs on a different thread, do not share a common
  // MojoMediaClient with other instances to avoid threading issues. Hence using
  // a unique_ptr here.
  std::unique_ptr<MojoMediaClient> mojo_media_client_;

  scoped_refptr<MediaLog> media_log_;
  shell::ServiceContextRefFactory ref_factory_;

  mojo::BindingSet<mojom::MediaService> bindings_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_H_
