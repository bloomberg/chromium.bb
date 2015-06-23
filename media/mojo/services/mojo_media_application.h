// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/mojo/services/mojo_renderer_service.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"
#include "url/gurl.h"

namespace media {

class MojoMediaApplication
    : public mojo::ApplicationDelegate,
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

  // mojo::InterfaceFactory<mojo::MediaRenderer> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::MediaRenderer> request) final;
};

}  // namespace media
