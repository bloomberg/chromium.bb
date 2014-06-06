// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node.h"

namespace mojo {
namespace examples {

class EmbeddedApp : public Application {
 public:
  EmbeddedApp() : view_manager_(NULL) {}
  virtual ~EmbeddedApp() {}

 private:
  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    view_manager::ViewManager::Create(this,
        base::Bind(&EmbeddedApp::OnRootAdded, base::Unretained(this)));
  }

  void OnRootAdded(view_manager::ViewManager* view_manager) {
    if (!view_manager_)
      view_manager_ = view_manager;
    DCHECK_EQ(view_manager_, view_manager);

    if (view_manager_->roots().size() == 1) {
      view_manager::View* view = view_manager::View::Create(view_manager_);
      view_manager_->roots().front()->SetActiveView(view);
      view->SetColor(SK_ColorYELLOW);
    } else {
      view_manager::View* view = view_manager::View::Create(view_manager_);
      view_manager_->roots().back()->SetActiveView(view);
      view->SetColor(SK_ColorRED);
    }
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