// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/services/view_manager/view_manager_init_service_impl.h"

namespace mojo {
namespace view_manager {
namespace service {

class ViewManagerApp : public ApplicationDelegate {
 public:
  ViewManagerApp() {}
  virtual ~ViewManagerApp() {}

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    // TODO(sky): this needs some sort of authentication as well as making sure
    // we only ever have one active at a time.
    connection->AddService<ViewManagerInitServiceImpl>();
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewManagerApp);
};

}  // namespace service
}  // namespace view_manager

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new mojo::view_manager::service::ViewManagerApp();
}

}  // namespace mojo
