// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application.h"
#include "mojo/services/view_manager/root_node_manager.h"
#include "mojo/services/view_manager/view_manager_connection.h"

namespace mojo {
namespace view_manager {
namespace service {

class ViewManagerApp : public Application {
 public:
  ViewManagerApp() {}
  virtual ~ViewManagerApp() {}

  virtual void Initialize() MOJO_OVERRIDE {
    root_node_manager_.reset(new RootNodeManager(service_provider()));
    AddService<ViewManagerConnection>(root_node_manager_.get());
  }

 private:
  scoped_ptr<RootNodeManager> root_node_manager_;
  DISALLOW_COPY_AND_ASSIGN(ViewManagerApp);
};

}
}

// static
Application* Application::Create() {
  return new mojo::view_manager::service::ViewManagerApp();
}

}
