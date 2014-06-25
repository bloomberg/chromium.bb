// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

namespace mojo {
namespace examples {

class DemoLauncher : public ApplicationDelegate {
 public:
  DemoLauncher() {}
  virtual ~DemoLauncher() {}

 private:
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE {
    app->ConnectToService("mojo:mojo_view_manager", &view_manager_init_);
  }

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    view_manager_init_->EmbedRoot("mojo:mojo_window_manager",
                                  base::Bind(&DemoLauncher::OnConnect,
                                             base::Unretained(this)));
    return true;
  }

  void OnConnect(bool success) {}

  view_manager::ViewManagerInitServicePtr view_manager_init_;

  DISALLOW_COPY_AND_ASSIGN(DemoLauncher);
};

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::DemoLauncher;
}

}  // namespace mojo
