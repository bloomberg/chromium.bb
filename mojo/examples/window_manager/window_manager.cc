// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "mojo/examples/window_manager/window_manager.mojom.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node.h"
#include "ui/events/event_constants.h"

#if defined CreateWindow
#undef CreateWindow
#endif

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
  virtual void CloseWindow(view_manager::TransportNodeId node_id) OVERRIDE;

  WindowManager* window_manager_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerConnection);
};

class WindowManager : public Application,
                      public view_manager::ViewObserver {
 public:
  WindowManager() {}
  virtual ~WindowManager() {}

  void CloseWindow(view_manager::TransportNodeId node_id) {
    DCHECK(view_manager_);
    view_manager::ViewTreeNode* node = view_manager_->GetNodeById(node_id);
    DCHECK(node);
    node->Destroy();
  }

 private:
  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    AddService<WindowManagerConnection>(this);

    view_manager_ = view_manager::ViewManager::CreateBlocking(this);
    view_manager::ViewTreeNode* node =
        view_manager::ViewTreeNode::Create(view_manager_);
    view_manager_->roots().front()->AddChild(node);
    node->SetBounds(gfx::Rect(800, 600));
    parent_node_id_ = node->id();

    view_manager::View* view = view_manager::View::Create(view_manager_);
    node->SetActiveView(view);
    view->SetColor(SK_ColorBLUE);
    view->AddObserver(this);
  }

    // Overridden from ViewObserver:
  virtual void OnViewInputEvent(view_manager::View* view,
                                EventPtr event) OVERRIDE {
    if (event->action == ui::ET_MOUSE_RELEASED)
      CreateWindow();
  }

  void CreateWindow() {
    view_manager::ViewTreeNode* node =
        view_manager_->GetNodeById(parent_node_id_);

    gfx::Rect bounds(50, 50, 200, 200);
    if (!node->children().empty()) {
      gfx::Point position = node->children().back()->bounds().origin();
      position.Offset(50, 50);
      bounds.set_origin(position);
    }

    view_manager::ViewTreeNode* embedded =
        view_manager::ViewTreeNode::Create(view_manager_);
    node->AddChild(embedded);
    embedded->SetBounds(bounds);
    embedded->Embed("mojo:mojo_embedded_app");
  }

  view_manager::ViewManager* view_manager_;
  view_manager::TransportNodeId parent_node_id_;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

void WindowManagerConnection::CloseWindow(
    view_manager::TransportNodeId node_id) {
  window_manager_->CloseWindow(node_id);
}

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::WindowManager;
}

}  // namespace mojo