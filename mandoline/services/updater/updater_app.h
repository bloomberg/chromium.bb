// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_SERVICES_UPDATER_UPDATER_APP_H_
#define MANDOLINE_SERVICES_UPDATER_UPDATER_APP_H_

#include "base/macros.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace mojo {
class Connection;
class Shell;
}  // namespace mojo

namespace updater {

class Updater;

class UpdaterApp : public mojo::ShellClient,
                   public mojo::InterfaceFactory<Updater> {
 public:
  UpdaterApp();
  ~UpdaterApp() override;

  // mojo::ShellClient:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  // InterfaceFactory<Updater> implementation.
  void Create(mojo::Connection* connection,
              mojo::InterfaceRequest<Updater> request) override;

 private:
  mojo::Shell* shell_;

  DISALLOW_COPY_AND_ASSIGN(UpdaterApp);
};

}  // namespace updater

#endif  // MANDOLINE_SERVICES_UPDATER_UPDATER_APP_H_
