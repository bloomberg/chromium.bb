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
#include "ui/events/event_constants.h"

#if defined CreateWindow
#undef CreateWindow
#endif

using mojo::view_manager::TransportNodeId;
using mojo::view_manager::View;
using mojo::view_manager::ViewManager;
using mojo::view_manager::ViewManagerDelegate;
using mojo::view_manager::ViewObserver;
using mojo::view_manager::ViewTreeNode;
using mojo::view_manager::ViewTreeNodeObserver;

namespace mojo {
namespace examples {

class WindowManager;

class WindowManagerConnection : public InterfaceImpl<IWindowManager> {
 public:
  explicit WindowManagerConnection(WindowManager* window_manager)
      : window_manager_(window_manager) {}
  virtual ~WindowManagerConnection() {}

 private:
  // Overridden from IWindowManager:
  virtual void CloseWindow(TransportNodeId node_id) OVERRIDE;

  WindowManager* window_manager_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerConnection);
};

class WindowManager : public Application,
                      public ViewObserver,
                      public ViewManagerDelegate {
 public:
  WindowManager() : view_manager_(NULL) {}
  virtual ~WindowManager() {}

  void CloseWindow(TransportNodeId node_id) {
    ViewTreeNode* node = view_manager_->GetNodeById(node_id);
    DCHECK(node);
    node->Destroy();
  }

 private:
  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    AddService<WindowManagerConnection>(this);
    ViewManager::Create(this, this);
  }

    // Overridden from ViewObserver:
  virtual void OnViewInputEvent(View* view, EventPtr event) OVERRIDE {
    if (event->action == ui::ET_MOUSE_RELEASED) {
      if (event->flags & ui::EF_LEFT_MOUSE_BUTTON)
        CreateWindow("mojo:mojo_embedded_app");
      else if (event->flags & ui::EF_RIGHT_MOUSE_BUTTON)
        CreateWindow("mojo:mojo_nesting_app");
    }
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnRootAdded(ViewManager* view_manager,
                           ViewTreeNode* root) OVERRIDE {
    DCHECK(!view_manager_);
    view_manager_ = view_manager;

    ViewTreeNode* node = ViewTreeNode::Create(view_manager);
    view_manager->roots().front()->AddChild(node);
    node->SetBounds(gfx::Rect(800, 600));
    parent_node_id_ = node->id();

    View* view = View::Create(view_manager);
    node->SetActiveView(view);
    view->SetColor(SK_ColorBLUE);
    view->AddObserver(this);
  }

  void CreateWindow(const String& url) {
    ViewTreeNode* node = view_manager_->GetNodeById(parent_node_id_);

    gfx::Rect bounds(50, 50, 200, 200);
    if (!node->children().empty()) {
      gfx::Point position = node->children().back()->bounds().origin();
      position.Offset(50, 50);
      bounds.set_origin(position);
    }

    ViewTreeNode* embedded = ViewTreeNode::Create(view_manager_);
    node->AddChild(embedded);
    embedded->SetBounds(bounds);
    embedded->Embed(url);
  }

  ViewManager* view_manager_;
  TransportNodeId parent_node_id_;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

void WindowManagerConnection::CloseWindow(TransportNodeId node_id) {
  window_manager_->CloseWindow(node_id);
}

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::WindowManager;
}

}  // namespace mojo