// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/mojo/interfaces/service_factory.mojom.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/message_loop_ref.h"
#include "services/shell/public/cpp/shell_client.h"
#include "url/gurl.h"

namespace media {

class MediaLog;
class MojoMediaClient;

class MojoMediaApplication
    : public shell::ShellClient,
      public shell::InterfaceFactory<interfaces::ServiceFactory> {
 public:
  explicit MojoMediaApplication(
      std::unique_ptr<MojoMediaClient> mojo_media_client);
  ~MojoMediaApplication() final;

 private:
  // shell::ShellClient implementation.
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) final;
  bool AcceptConnection(shell::Connection* connection) final;
  bool ShellConnectionLost() final;

  // shell::InterfaceFactory<interfaces::ServiceFactory> implementation.
  void Create(shell::Connection* connection,
              mojo::InterfaceRequest<interfaces::ServiceFactory> request) final;

  // Note: Since each instance runs on a different thread, do not share a common
  // MojoMediaClient with other instances to avoid threading issues. Hence using
  // a unique_ptr here.
  std::unique_ptr<MojoMediaClient> mojo_media_client_;

  shell::Connector* connector_;
  scoped_refptr<MediaLog> media_log_;
  shell::MessageLoopRefFactory ref_factory_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_H_
