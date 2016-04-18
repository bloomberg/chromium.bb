// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MEDIA_CAST_MOJO_MEDIA_APPLICATION_H_
#define CHROMECAST_BROWSER_MEDIA_CAST_MOJO_MEDIA_APPLICATION_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "media/mojo/interfaces/service_factory.mojom.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/message_loop_ref.h"
#include "services/shell/public/cpp/shell_client.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
class MediaLog;
}  // namespace media

namespace chromecast {
namespace media {

class CastMojoMediaClient;

class CastMojoMediaApplication
    : public ::shell::ShellClient,
      public ::shell::InterfaceFactory<::media::interfaces::ServiceFactory> {
 public:
  CastMojoMediaApplication(
      std::unique_ptr<CastMojoMediaClient> mojo_media_client,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner);
  ~CastMojoMediaApplication() final;

 private:
  // ::shell::ShellClient implementation.
  void Initialize(::shell::Connector* connector,
                  const ::shell::Identity& identity,
                  uint32_t id) final;
  bool AcceptConnection(::shell::Connection* connection) final;

  // ::shell::InterfaceFactory<interfaces::ServiceFactory> implementation.
  void Create(::shell::Connection* connection,
              mojo::InterfaceRequest<::media::interfaces::ServiceFactory>
                  request) final;

  std::unique_ptr<CastMojoMediaClient> mojo_media_client_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<::media::MediaLog> media_log_;
  ::shell::MessageLoopRefFactory ref_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_MEDIA_CAST_MOJO_MEDIA_APPLICATION_H_
