// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/examples/window_manager/window_manager.mojom.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

namespace mojo {
namespace examples {

namespace {
const char kEmbeddedAppURL[] = "mojo:mojo_embedded_app";
}

class NestingApp;

// An app that embeds another app.
// TODO(davemoore): Is this the right name?
class NestingApp
    : public ApplicationDelegate,
      public ViewManagerDelegate,
      public ViewObserver {
 public:
  NestingApp() : nested_(NULL) {}
  virtual ~NestingApp() {}

 private:
  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE {
    view_manager_client_factory_.reset(
        new ViewManagerClientFactory(app->shell(), this));
  }

  // Overridden from ApplicationImpl:
  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    connection->ConnectToService(&window_manager_);
    connection->AddService(view_manager_client_factory_.get());
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

  scoped_ptr<ViewManagerClientFactory> view_manager_client_factory_;

  View* nested_;
  IWindowManagerPtr window_manager_;

  DISALLOW_COPY_AND_ASSIGN(NestingApp);
};

}  // namespace examples
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::examples::NestingApp);
  return runner.Run(shell_handle);
}
