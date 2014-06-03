// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node.h"

namespace mojo {
namespace examples {

class EmbeddedApp : public Application {
 public:
  EmbeddedApp() {}
  virtual ~EmbeddedApp() {}

 private:
  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    view_manager_ = view_manager::ViewManager::CreateBlocking(this);
    view_manager::View* view = view_manager::View::Create(view_manager_);
    view_manager_->tree()->SetActiveView(view);
    view->SetColor(SK_ColorYELLOW);
  }

  view_manager::ViewManager* view_manager_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedApp);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::EmbeddedApp;
}

}  // namespace mojo