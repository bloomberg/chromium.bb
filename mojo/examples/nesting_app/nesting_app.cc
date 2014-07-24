// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "mojo/examples/window_manager/window_manager.mojom.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory_with_context.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/node_observer.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

using mojo::view_manager::Node;
using mojo::view_manager::NodeObserver;
using mojo::view_manager::View;
using mojo::view_manager::ViewManager;
using mojo::view_manager::ViewManagerClientFactory;
using mojo::view_manager::ViewManagerDelegate;
using mojo::view_manager::ViewObserver;

namespace mojo {
namespace examples {

namespace {
const char kEmbeddedAppURL[] = "mojo:mojo_embedded_app";
}

class NestingApp;

class Navigator : public InterfaceImpl<navigation::Navigator> {
 public:
  explicit Navigator(NestingApp* app) : app_(app) {}

 private:
  virtual void Navigate(
      uint32 node_id,
      navigation::NavigationDetailsPtr navigation_details,
      navigation::ResponseDetailsPtr response_details) OVERRIDE;

  NestingApp* app_;
  DISALLOW_COPY_AND_ASSIGN(Navigator);
};

// An app that embeds another app.
// TODO(davemoore): Is this the right name?
class NestingApp : public ApplicationDelegate,
                   public ViewManagerDelegate,
                   public ViewObserver,
                   public NodeObserver,
                   public InterfaceFactoryWithContext<Navigator, NestingApp> {
 public:
  NestingApp()
      : InterfaceFactoryWithContext(this),
        nested_(NULL),
        view_manager_client_factory_(this) {}
  virtual ~NestingApp() {}

  void set_color(const std::string& color) { color_ = color; }

  void NavigateChild() {
    if (!color_.empty() && nested_) {
      navigation::NavigationDetailsPtr details(
          navigation::NavigationDetails::New());
      details->url =
          base::StringPrintf("%s/%s", kEmbeddedAppURL, color_.c_str());
      navigation::ResponseDetailsPtr response_details(
          navigation::ResponseDetails::New());
      navigator_->Navigate(
          nested_->id(), details.Pass(), response_details.Pass());
    }
  }

 private:
  // Overridden from ApplicationImpl:
  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    connection->ConnectToService(&window_manager_);
    connection->AddService(&view_manager_client_factory_);
    connection->AddService(this);
    // TODO(davemoore): Is this ok?
    if (!navigator_) {
      connection->ConnectToApplication(
          kEmbeddedAppURL)->ConnectToService(&navigator_);
    }
    return true;
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnRootAdded(ViewManager* view_manager, Node* root) OVERRIDE {
    root->AddObserver(this);

    View* view = View::Create(view_manager);
    root->SetActiveView(view);
    view->SetColor(SK_ColorCYAN);
    view->AddObserver(this);

    nested_ = Node::Create(view_manager);
    root->AddChild(nested_);
    nested_->SetBounds(gfx::Rect(20, 20, 50, 50));
    nested_->Embed(kEmbeddedAppURL);

    NavigateChild();
  }
  virtual void OnViewManagerDisconnected(ViewManager* view_manager) OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

  // Overridden from ViewObserver:
  virtual void OnViewInputEvent(View* view, const EventPtr& event) OVERRIDE {
    if (event->action == EVENT_TYPE_MOUSE_RELEASED)
      window_manager_->CloseWindow(view->node()->id());
  }

  // Overridden from NodeObserver:
  virtual void OnNodeDestroyed(Node* node) OVERRIDE {
    // TODO(beng): reap views & child nodes.
    nested_ = NULL;
  }

  std::string color_;
  Node* nested_;
  navigation::NavigatorPtr navigator_;
  IWindowManagerPtr window_manager_;
  ViewManagerClientFactory view_manager_client_factory_;

  DISALLOW_COPY_AND_ASSIGN(NestingApp);
};

void Navigator::Navigate(uint32 node_id,
                         navigation::NavigationDetailsPtr navigation_details,
                         navigation::ResponseDetailsPtr response_details) {
  GURL url(navigation_details->url.To<std::string>());
  if (!url.is_valid()) {
    LOG(ERROR) << "URL is invalid.";
    return;
  }
  app_->set_color(url.path().substr(1));
  app_->NavigateChild();
}

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::NestingApp;
}

}  // namespace mojo
