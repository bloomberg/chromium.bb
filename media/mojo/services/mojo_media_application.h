// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/mojo/interfaces/service_factory.mojom.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/message_loop_ref.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "url/gurl.h"

namespace media {

class MediaLog;
class MojoMediaClient;

class MojoMediaApplication
    : public mojo::ShellClient,
      public mojo::InterfaceFactory<interfaces::ServiceFactory> {
 public:
  explicit MojoMediaApplication(scoped_ptr<MojoMediaClient> mojo_media_client);
  ~MojoMediaApplication() final;

 private:
  // mojo::ShellClient implementation.
  void Initialize(mojo::Connector* connector,
                  const mojo::Identity& identity,
                  uint32_t id) final;
  bool AcceptConnection(mojo::Connection* connection) final;

  // mojo::InterfaceFactory<interfaces::ServiceFactory> implementation.
  void Create(mojo::Connection* connection,
              mojo::InterfaceRequest<interfaces::ServiceFactory> request) final;

  // Note: Since each instance runs on a different thread, do not share a common
  // MojoMediaClient with other instances to avoid threading issues. Hence using
  // a scoped_ptr here.
  scoped_ptr<MojoMediaClient> mojo_media_client_;

  mojo::Connector* connector_;
  scoped_refptr<MediaLog> media_log_;
  mojo::MessageLoopRefFactory ref_factory_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_H_
