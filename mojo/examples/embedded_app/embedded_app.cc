// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
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
#include "url/url_util.h"

namespace mojo {
namespace examples {

const SkColor kColors[] = {SK_ColorYELLOW, SK_ColorRED, SK_ColorGREEN,
                           SK_ColorMAGENTA};

class EmbeddedApp;

class EmbeddedApp
    : public ApplicationDelegate,
      public ViewManagerDelegate,
      public ViewObserver {
 public:
  EmbeddedApp() { url::AddStandardScheme("mojo"); }
  virtual ~EmbeddedApp() {}

 private:

  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE {
    view_manager_client_factory_.reset(
        new ViewManagerClientFactory(app->shell(), this));

    // TODO(aa): Weird for embeddee to talk to embedder by URL. Seems like
    // embedder should be able to specify the SP embeddee receives, then
    // communication can be anonymous.
    app->ConnectToService("mojo:mojo_window_manager", &navigator_host_);
  }

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    connection->AddService(view_manager_client_factory_.get());
    return true;
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnEmbed(ViewManager* view_manager,
                       View* root,
                       ServiceProviderImpl* exported_services,
                       scoped_ptr<ServiceProvider> imported_services) OVERRIDE {
    root->AddObserver(this);
    roots_[root->id()] = root;
    root->SetColor(kColors[next_color_++ % arraysize(kColors)]);
  }
  virtual void OnViewManagerDisconnected(ViewManager* view_manager) OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

  // Overridden from ViewObserver:
  virtual void OnViewDestroyed(View* view) OVERRIDE {
    DCHECK(roots_.find(view->id()) != roots_.end());
    roots_.erase(view->id());
  }
  virtual void OnViewInputEvent(View* view, const EventPtr& event) OVERRIDE {
    if (event->action == EVENT_TYPE_MOUSE_RELEASED) {
      if (event->flags & EVENT_FLAGS_LEFT_MOUSE_BUTTON) {
        NavigationDetailsPtr nav_details(NavigationDetails::New());
        nav_details->request->url =
            "http://www.aaronboodman.com/z_dropbox/test.html";
        navigator_host_->RequestNavigate(view->id(), TARGET_SOURCE_NODE,
                                         nav_details.Pass());
      }
    }
  }

  NavigatorHostPtr navigator_host_;
  scoped_ptr<ViewManagerClientFactory> view_manager_client_factory_;

  typedef std::map<Id, View*> RootMap;
  RootMap roots_;

  int next_color_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedApp);
};

}  // namespace examples
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::examples::EmbeddedApp);
  return runner.Run(shell_handle);
}
