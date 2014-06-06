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

class WindowManager : public Application {
 public:
  WindowManager() {}
  virtual ~WindowManager() {}

 private:
  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    view_manager_ = view_manager::ViewManager::CreateBlocking(this,
        base::Bind(&WindowManager::OnRootAdded, base::Unretained(this)));
    view_manager::ViewTreeNode* node =
        view_manager::ViewTreeNode::Create(view_manager_);
    view_manager_->roots().front()->AddChild(node);
    node->SetBounds(gfx::Rect(800, 600));

    view_manager::View* view = view_manager::View::Create(view_manager_);
    node->SetActiveView(view);
    view->SetColor(SK_ColorBLUE);

    view_manager::ViewTreeNode* embedded =
        view_manager::ViewTreeNode::Create(view_manager_);
    node->AddChild(embedded);
    embedded->SetBounds(gfx::Rect(50, 50, 200, 200));
    embedded->Embed("mojo:mojo_embedded_app");

    view_manager::ViewTreeNode* embedded2 =
        view_manager::ViewTreeNode::Create(view_manager_);
    node->AddChild(embedded2);
    embedded2->SetBounds(gfx::Rect(250, 250, 200, 200));
    embedded2->Embed("mojo:mojo_embedded_app");
  }

  void OnRootAdded(view_manager::ViewManager* view_manager) {}

  view_manager::ViewManager* view_manager_;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::WindowManager;
}

}  // namespace mojo