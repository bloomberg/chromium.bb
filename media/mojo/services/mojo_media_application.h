// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/interfaces/media_renderer.mojom.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"
#include "url/gurl.h"

namespace media {

class MojoMediaApplication
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mojo::ContentDecryptionModule>,
      public mojo::InterfaceFactory<mojo::MediaRenderer> {
 public:
  static GURL AppUrl();
  static scoped_ptr<mojo::ApplicationDelegate> CreateApp();

  MojoMediaApplication();
  ~MojoMediaApplication() final;

 private:
  // mojo::ApplicationDelegate implementation.
  void Initialize(mojo::ApplicationImpl* app) final;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) final;

  // mojo::InterfaceFactory<mojo::ContentDecryptionModule> implementation.
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mojo::ContentDecryptionModule> request) final;

  // mojo::InterfaceFactory<mojo::MediaRenderer> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::MediaRenderer> request) final;

  MojoCdmServiceContext cdm_service_context_;
};

}  // namespace media
