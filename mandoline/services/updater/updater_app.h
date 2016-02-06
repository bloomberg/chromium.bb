// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_SERVICES_UPDATER_UPDATER_APP_H_
#define MANDOLINE_SERVICES_UPDATER_UPDATER_APP_H_

#include "base/macros.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/interface_factory.h"

namespace mojo {
class ApplicationConnection;
class ApplicationImpl;
}  // namespace mojo

namespace updater {

class Updater;

class UpdaterApp : public mojo::ApplicationDelegate,
                   public mojo::InterfaceFactory<Updater> {
 public:
  UpdaterApp();
  ~UpdaterApp() override;

  void Initialize(mojo::ApplicationImpl* app) override;

  // mojo::ApplicationDelegate:
  bool AcceptConnection(
      mojo::ApplicationConnection* connection) override;

  // InterfaceFactory<Updater> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<Updater> request) override;

 private:
  mojo::ApplicationImpl* app_impl_;

  DISALLOW_COPY_AND_ASSIGN(UpdaterApp);
};

}  // namespace updater

#endif  // MANDOLINE_SERVICES_UPDATER_UPDATER_APP_H_
