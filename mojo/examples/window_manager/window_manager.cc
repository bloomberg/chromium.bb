// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "mojo/examples/window_manager/window_manager.mojom.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/navigation/navigation.mojom.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node.h"
#include "ui/events/event_constants.h"

#if defined CreateWindow
#undef CreateWindow
#endif

using mojo::view_manager::Id;
using mojo::view_manager::View;
using mojo::view_manager::ViewManager;
using mojo::view_manager::ViewManagerDelegate;
using mojo::view_manager::ViewObserver;
using mojo::view_manager::ViewTreeNode;
using mojo::view_manager::ViewTreeNodeObserver;

namespace mojo {
namespace examples {

class WindowManager;

namespace {

const SkColor kColors[] = { SK_ColorYELLOW,
                            SK_ColorRED,
                            SK_ColorGREEN,
                            SK_ColorMAGENTA };

const char kEmbeddedAppURL[] = "mojo:mojo_embedded_app";
const char kNestingAppURL[] = "mojo:mojo_nesting_app";
const char kMojoBrowserURL[] = "mojo:mojo_browser";

}  // namespace

class WindowManagerConnection : public InterfaceImpl<IWindowManager> {
 public:
  explicit WindowManagerConnection(WindowManager* window_manager)
      : window_manager_(window_manager) {}
  virtual ~WindowManagerConnection() {}

 private:
  // Overridden from IWindowManager:
  virtual void CloseWindow(Id node_id) OVERRIDE;

  WindowManager* window_manager_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerConnection);
};

class WindowManager : public Application,
                      public ViewObserver,
                      public ViewManagerDelegate {
 public:
  WindowManager() : view_manager_(NULL) {}
  virtual ~WindowManager() {}

  void CloseWindow(Id node_id) {
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
  virtual void OnViewInputEvent(View* view, const EventPtr& event) OVERRIDE {
    if (event->action == ui::ET_MOUSE_RELEASED) {
      if (event->flags & ui::EF_LEFT_MOUSE_BUTTON)
        CreateWindow(kEmbeddedAppURL);
      else if (event->flags & ui::EF_RIGHT_MOUSE_BUTTON)
        CreateWindow(kNestingAppURL);
      else if (event->flags & ui::EF_MIDDLE_MOUSE_BUTTON)
        CreateWindow(kMojoBrowserURL);
    }
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnRootAdded(ViewManager* view_manager,
                           ViewTreeNode* root) OVERRIDE {
    DCHECK(!view_manager_);
    view_manager_ = view_manager;

    ViewTreeNode* node = ViewTreeNode::Create(view_manager);
    view_manager->GetRoots().front()->AddChild(node);
    node->SetBounds(gfx::Rect(800, 600));
    parent_node_id_ = node->id();

    View* view = View::Create(view_manager);
    node->SetActiveView(view);
    view->SetColor(SK_ColorBLUE);
    view->AddObserver(this);
  }

  void CreateWindow(const std::string& url) {
    ViewTreeNode* node = view_manager_->GetNodeById(parent_node_id_);

    gfx::Rect bounds(50, 50, 400, 400);
    if (!node->children().empty()) {
      gfx::Point position = node->children().back()->bounds().origin();
      position.Offset(50, 50);
      bounds.set_origin(position);
    }

    ViewTreeNode* embedded = ViewTreeNode::Create(view_manager_);
    node->AddChild(embedded);
    embedded->SetBounds(bounds);
    embedded->Embed(url);

    // TODO(aa): Is there a way to ask for an interface and test whether it
    // succeeded? That would be nicer than hard-coding the URLs that are known
    // to support navigation.
    if (url == kEmbeddedAppURL || url == kNestingAppURL) {
      // TODO(aa): This means that there can only ever be one instance of every
      // app, which seems wrong. Instead, perhaps embedder should get back a
      // service provider that allows it to talk to embeddee.
      navigation::NavigatorPtr navigator;
      ConnectTo(url, &navigator);
      navigation::NavigationDetailsPtr details(
          navigation::NavigationDetails::New());
      size_t index = node->children().size() - 1;
      details->url = base::StringPrintf(
          "%s/%x", kEmbeddedAppURL, kColors[index % arraysize(kColors)]);
      navigator->Navigate(embedded->id(), details.Pass());
    }
  }

  ViewManager* view_manager_;
  Id parent_node_id_;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

void WindowManagerConnection::CloseWindow(Id node_id) {
  window_manager_->CloseWindow(node_id);
}

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::WindowManager;
}

}  // namespace mojo
