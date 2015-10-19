// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "media/mojo/interfaces/service_factory.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"
#include "url/gurl.h"

namespace media {

class MediaLog;

class MojoMediaApplication
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<interfaces::ServiceFactory> {
 public:
  static scoped_ptr<mojo::ApplicationDelegate> CreateApp();

  explicit MojoMediaApplication(bool enable_logging);
  ~MojoMediaApplication() final;

 private:
  // mojo::ApplicationDelegate implementation.
  void Initialize(mojo::ApplicationImpl* app) final;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) final;

  // mojo::InterfaceFactory<interfaces::ServiceFactory> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<interfaces::ServiceFactory> request) final;

  bool enable_logging_;
  mojo::ApplicationImpl* app_impl_;
  scoped_refptr<MediaLog> media_log_;
};

}  // namespace media
