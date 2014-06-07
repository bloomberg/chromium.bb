// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "mojo/examples/window_manager/window_manager.mojom.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node_observer.h"
#include "ui/events/event_constants.h"

using mojo::view_manager::View;
using mojo::view_manager::ViewManager;
using mojo::view_manager::ViewManagerDelegate;
using mojo::view_manager::ViewObserver;
using mojo::view_manager::ViewTreeNode;
using mojo::view_manager::ViewTreeNodeObserver;

namespace mojo {
namespace examples {

// An app that embeds another app.
class NestingApp : public Application,
                   public ViewManagerDelegate,
                   public ViewObserver {
 public:
  NestingApp() {}
  virtual ~NestingApp() {}

 private:
  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    ViewManager::Create(this, this);
    ConnectTo<IWindowManager>("mojo:mojo_window_manager", &window_manager_);
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnRootAdded(ViewManager* view_manager,
                           ViewTreeNode* root) OVERRIDE {
    View* view = View::Create(view_manager);
    root->SetActiveView(view);
    view->SetColor(SK_ColorCYAN);
    view->AddObserver(this);

    ViewTreeNode* nested = ViewTreeNode::Create(view_manager);
    root->AddChild(nested);
    nested->SetBounds(gfx::Rect(20, 20, 50, 50));
    nested->Embed("mojo:mojo_embedded_app");
  }
  virtual void OnRootRemoved(ViewManager* view_manager,
                             ViewTreeNode* root) OVERRIDE {
    // TODO(beng): reap views & child nodes.
  }

  // Overridden from ViewObserver:
  virtual void OnViewInputEvent(View* view, EventPtr event) OVERRIDE {
    if (event->action == ui::ET_MOUSE_RELEASED)
      window_manager_->CloseWindow(view->node()->id());
  }

  IWindowManagerPtr window_manager_;

  DISALLOW_COPY_AND_ASSIGN(NestingApp);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::NestingApp;
}

}  // namespace mojo