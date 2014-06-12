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
#include "mojo/services/public/cpp/view_manager/view_tree_node_observer.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

using mojo::view_manager::View;
using mojo::view_manager::ViewManager;
using mojo::view_manager::ViewManagerDelegate;
using mojo::view_manager::ViewObserver;
using mojo::view_manager::ViewTreeNode;
using mojo::view_manager::ViewTreeNodeObserver;

namespace mojo {
namespace examples {

namespace {
const char kEmbeddedAppURL[] = "mojo:mojo_embedded_app";
}

// An app that embeds another app.
class NestingApp : public Application,
                   public ViewManagerDelegate,
                   public ViewObserver {
 public:
  NestingApp() : nested_(NULL) {}
  virtual ~NestingApp() {}

 private:
  class Navigator : public InterfaceImpl<navigation::Navigator> {
   public:
    explicit Navigator(NestingApp* app) : app_(app) {}
   private:
    virtual void Navigate(uint32 node_id,
                          navigation::NavigationDetailsPtr details) OVERRIDE {
      GURL url(details->url.To<std::string>());
      if (!url.is_valid()) {
        LOG(ERROR) << "URL is invalid.";
        return;
      }
      app_->color_ = url.path().substr(1);
      app_->NavigateChild();
    }
    NestingApp* app_;
    DISALLOW_COPY_AND_ASSIGN(Navigator);
  };

  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    ViewManager::Create(this, this);
    ConnectTo<IWindowManager>("mojo:mojo_window_manager", &window_manager_);
    AddService<Navigator>(this);
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnRootAdded(ViewManager* view_manager,
                           ViewTreeNode* root) OVERRIDE {
    View* view = View::Create(view_manager);
    root->SetActiveView(view);
    view->SetColor(SK_ColorCYAN);
    view->AddObserver(this);

    nested_ = ViewTreeNode::Create(view_manager);
    root->AddChild(nested_);
    nested_->SetBounds(gfx::Rect(20, 20, 50, 50));
    nested_->Embed(kEmbeddedAppURL);

    if (!navigator_.get())
      ConnectTo(kEmbeddedAppURL, &navigator_);

    NavigateChild();
  }

  virtual void OnRootRemoved(ViewManager* view_manager,
                             ViewTreeNode* root) OVERRIDE {
    // TODO(beng): reap views & child nodes.
    nested_ = NULL;
  }

  // Overridden from ViewObserver:
  virtual void OnViewInputEvent(View* view, const EventPtr& event) OVERRIDE {
    if (event->action == ui::ET_MOUSE_RELEASED)
      window_manager_->CloseWindow(view->node()->id());
  }

  void NavigateChild() {
    if (!color_.empty() && nested_) {
      navigation::NavigationDetailsPtr details(
          navigation::NavigationDetails::New());
      details->url =
          base::StringPrintf("%s/%s", kEmbeddedAppURL, color_.c_str());
      navigator_->Navigate(nested_->id(), details.Pass());
    }
  }

  std::string color_;
  ViewTreeNode* nested_;
  navigation::NavigatorPtr navigator_;
  IWindowManagerPtr window_manager_;

  DISALLOW_COPY_AND_ASSIGN(NestingApp);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::NestingApp;
}

}  // namespace mojo
