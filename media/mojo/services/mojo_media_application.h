// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/mojo/interfaces/service_factory.mojom.h"
#include "mojo/shell/public/cpp/interface_factory_impl.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "url/gurl.h"

namespace media {

class MediaLog;
class MojoMediaClient;

class MojoMediaApplication
    : public mojo::ShellClient,
      public mojo::InterfaceFactory<interfaces::ServiceFactory> {
 public:
  // Callback to create a MojoMediaClient.
  using CreateMojoMediaClientCB = base::Callback<scoped_ptr<MojoMediaClient>()>;

  // Creates MojoMediaApplication using the default MojoMediaClient.
  static scoped_ptr<mojo::ShellClient> CreateApp();

  // Creates MojoMediaApplication using the MojoMediaClient provided by
  // |create_mojo_media_client_cb|.
  static scoped_ptr<mojo::ShellClient> CreateAppWithClient(
      const CreateMojoMediaClientCB& create_mojo_media_client_cb);

  ~MojoMediaApplication() final;

 private:
  explicit MojoMediaApplication(scoped_ptr<MojoMediaClient> mojo_media_client);

  // mojo::ShellClient implementation.
  void Initialize(mojo::Shell* shell,
                  const std::string& url,
                  uint32_t id) final;
  bool AcceptConnection(mojo::Connection* connection) final;

  // mojo::InterfaceFactory<interfaces::ServiceFactory> implementation.
  void Create(mojo::Connection* connection,
              mojo::InterfaceRequest<interfaces::ServiceFactory> request) final;

  // Note: Since each instance runs on a different thread, do not share a common
  // MojoMediaClient with other instances to avoid threading issues. Hence using
  // a scoped_ptr here.
  scoped_ptr<MojoMediaClient> mojo_media_client_;

  mojo::Shell* shell_;
  scoped_refptr<MediaLog> media_log_;
};

}  // namespace media
