// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/application_runner_chromium.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/cpp/view_manager/window_manager_delegate.h"
#include "mojo/services/public/interfaces/input_events/input_events.mojom.h"
#include "mojo/services/window_manager/window_manager_app.h"

namespace examples {

class SimpleWM : public mojo::ApplicationDelegate,
                 public mojo::ViewManagerDelegate,
                 public mojo::WindowManagerDelegate,
                 public mojo::ViewObserver {
 public:
  SimpleWM()
      : window_manager_app_(new mojo::WindowManagerApp(this, this)),
        view_manager_(NULL),
        root_(NULL),
        window_container_(NULL),
        next_window_origin_(10, 10) {}
  virtual ~SimpleWM() {}

 private:
  // Overridden from mojo::ApplicationDelegate:
  virtual void Initialize(mojo::ApplicationImpl* impl) MOJO_OVERRIDE {
    window_manager_app_->Initialize(impl);
  }
  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) MOJO_OVERRIDE {
    window_manager_app_->ConfigureIncomingConnection(connection);
    return true;
  }

  // Overridden from mojo::ViewManagerDelegate:
  virtual void OnEmbed(
      mojo::ViewManager* view_manager,
      mojo::View* root,
      mojo::ServiceProviderImpl* exported_services,
      scoped_ptr<mojo::ServiceProvider> remote_service_provider) MOJO_OVERRIDE {
    view_manager_ = view_manager;
    root_ = root;

    window_container_ = mojo::View::Create(view_manager_);
    window_container_->SetBounds(root_->bounds());
    root_->AddChild(window_container_);

  }
  virtual void OnViewManagerDisconnected(
      mojo::ViewManager* view_manager) MOJO_OVERRIDE {
    view_manager_ = NULL;
    root_ = NULL;
  }

  // Overridden from mojo::WindowManagerDelegate:
  virtual void Embed(
      const mojo::String& url,
      mojo::InterfaceRequest<mojo::ServiceProvider> service_provider)
          MOJO_OVERRIDE {
    mojo::View* frame_view = mojo::View::Create(view_manager_);
    window_container_->AddChild(frame_view);
    frame_view->SetBounds(gfx::Rect(next_window_origin_, gfx::Size(400, 400)));
    frame_view->SetColor(SK_ColorBLUE);
    frame_view->AddObserver(this);

    mojo::View* embed_view = mojo::View::Create(view_manager_);
    gfx::Rect client_bounds(frame_view->bounds().size());
    client_bounds.Inset(10, 30, 10, 10);
    embed_view->SetBounds(client_bounds);
    frame_view->AddChild(embed_view);

    // TODO(beng): We're dropping the |service_provider| passed from the client
    //             on the floor here and passing our own. Seems like we should
    //             be sending both. I'm not yet sure how this sould work for
    //             N levels of proxying.
    embed_view->Embed(url, scoped_ptr<mojo::ServiceProviderImpl>(
        new mojo::ServiceProviderImpl).Pass());
    next_window_origin_.Offset(50, 50);
  }
  virtual void DispatchEvent(mojo::EventPtr event) MOJO_OVERRIDE {}

  // Overridden from mojo::ViewObserver:
  virtual void OnViewInputEvent(mojo::View* view,
                                const mojo::EventPtr& event) MOJO_OVERRIDE {
    if (event->action == mojo::EVENT_TYPE_MOUSE_RELEASED &&
        event->flags & mojo::EVENT_FLAGS_RIGHT_MOUSE_BUTTON &&
        view->parent() == window_container_) {
      CloseWindow(view);
    }
  }
  virtual void OnViewDestroyed(mojo::View* view) MOJO_OVERRIDE {
    view->RemoveObserver(this);
  }

  void CloseWindow(mojo::View* view) {
    mojo::View* first_child = view->children().front();
    first_child->Destroy();
    view->Destroy();
    next_window_origin_.Offset(-50, -50);
  }

  scoped_ptr<mojo::WindowManagerApp> window_manager_app_;

  mojo::ViewManager* view_manager_;
  mojo::View* root_;
  mojo::View* window_container_;

  gfx::Point next_window_origin_;

  DISALLOW_COPY_AND_ASSIGN(SimpleWM);
};

}  // namespace examples

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new examples::SimpleWM);
  return runner.Run(shell_handle);
}
