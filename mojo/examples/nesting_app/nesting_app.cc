// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "mojo/examples/window_manager/window_manager.mojom.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_runner_chromium.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

namespace mojo {
namespace examples {

namespace {
const char kEmbeddedAppURL[] = "mojo:mojo_embedded_app";
}

class NestingApp;

class NavigatorImpl : public InterfaceImpl<Navigator> {
 public:
  explicit NavigatorImpl(NestingApp* app) : app_(app) {}

 private:
  virtual void Navigate(
      uint32 node_id,
      NavigationDetailsPtr navigation_details,
      ResponseDetailsPtr response_details) OVERRIDE;

  NestingApp* app_;
  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

// An app that embeds another app.
// TODO(davemoore): Is this the right name?
class NestingApp
    : public ApplicationDelegate,
      public ViewManagerDelegate,
      public ViewObserver {
 public:
  NestingApp()
      : navigator_factory_(this),
        view_manager_client_factory_(this),
        nested_(NULL) {}
  virtual ~NestingApp() {}

  void set_color(const std::string& color) { color_ = color; }

  void NavigateChild() {
    if (!color_.empty() && nested_) {
      NavigationDetailsPtr details(NavigationDetails::New());
      details->request->url =
          base::StringPrintf("%s/%s", kEmbeddedAppURL, color_.c_str());
      ResponseDetailsPtr response_details(ResponseDetails::New());
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
    connection->AddService(&navigator_factory_);
    // TODO(davemoore): Is this ok?
    if (!navigator_) {
      connection->ConnectToApplication(
          kEmbeddedAppURL)->ConnectToService(&navigator_);
    }
    return true;
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnEmbed(ViewManager* view_manager,
                       View* root,
                       ServiceProviderImpl* exported_services,
                       scoped_ptr<ServiceProvider> imported_services) OVERRIDE {
    root->AddObserver(this);
    root->SetColor(SK_ColorCYAN);

    nested_ = View::Create(view_manager);
    root->AddChild(nested_);
    nested_->SetBounds(gfx::Rect(20, 20, 50, 50));
    nested_->Embed(kEmbeddedAppURL);

    NavigateChild();
  }
  virtual void OnViewManagerDisconnected(ViewManager* view_manager) OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

  // Overridden from ViewObserver:
  virtual void OnViewDestroyed(View* view) OVERRIDE {
    // TODO(beng): reap views & child Views.
    nested_ = NULL;
  }
  virtual void OnViewInputEvent(View* view, const EventPtr& event) OVERRIDE {
    if (event->action == EVENT_TYPE_MOUSE_RELEASED)
      window_manager_->CloseWindow(view->id());
  }

  InterfaceFactoryImplWithContext<NavigatorImpl, NestingApp> navigator_factory_;
  ViewManagerClientFactory view_manager_client_factory_;

  std::string color_;
  View* nested_;
  NavigatorPtr navigator_;
  IWindowManagerPtr window_manager_;

  DISALLOW_COPY_AND_ASSIGN(NestingApp);
};

void NavigatorImpl::Navigate(uint32 node_id,
                             NavigationDetailsPtr navigation_details,
                             ResponseDetailsPtr response_details) {
  GURL url(navigation_details->request->url.To<std::string>());
  if (!url.is_valid()) {
    LOG(ERROR) << "URL is invalid.";
    return;
  }
  app_->set_color(url.path().substr(1));
  app_->NavigateChild();
}

}  // namespace examples
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::examples::NestingApp);
  return runner.Run(shell_handle);
}
