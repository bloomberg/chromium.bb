// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/window_manager_delegate.h"
#include "mojo/services/window_manager/window_manager_app.h"

namespace examples {

class SimpleWM : public mojo::ApplicationDelegate,
                 public mojo::ViewManagerDelegate,
                 public mojo::WindowManagerDelegate {
 public:
  SimpleWM()
      : window_manager_app_(new mojo::WindowManagerApp(this)),
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
    view_manager_->SetWindowManagerDelegate(this);

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
    mojo::View* embed_view = mojo::View::Create(view_manager_);
    embed_view->SetBounds(gfx::Rect(next_window_origin_, gfx::Size(400, 400)));
    window_container_->AddChild(embed_view);

    // TODO(beng): We're dropping the |service_provider| passed from the client
    //             on the floor here and passing our own. Seems like we should
    //             be sending both. I'm not yet sure how this sould work for
    //             N levels of proxying.
    embed_view->Embed(url, scoped_ptr<mojo::ServiceProviderImpl>(
        new mojo::ServiceProviderImpl).Pass());
    next_window_origin_.Offset(50, 50);
  }
  virtual void DispatchEvent(mojo::View* target,
                             mojo::EventPtr event) MOJO_OVERRIDE {
    view_manager_->DispatchEvent(target, event.Pass());
  }

  scoped_ptr<mojo::WindowManagerApp> window_manager_app_;

  mojo::ViewManager* view_manager_;
  mojo::View* root_;
  mojo::View* window_container_;

  gfx::Point next_window_origin_;

  DISALLOW_COPY_AND_ASSIGN(SimpleWM);
};

}  // namespace examples

namespace mojo {

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::SimpleWM;
}

}  // namespace
