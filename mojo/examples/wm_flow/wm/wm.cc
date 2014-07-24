// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/window_manager_delegate.h"
#include "mojo/services/window_manager/window_manager_app.h"

namespace examples {

class SimpleWM : public mojo::ApplicationDelegate,
                 public mojo::view_manager::ViewManagerDelegate,
                 public mojo::view_manager::WindowManagerDelegate {
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

  // Overridden from mojo::view_manager::ViewManagerDelegate:
  virtual void OnRootAdded(
      mojo::view_manager::ViewManager* view_manager,
      mojo::view_manager::Node* root) MOJO_OVERRIDE {
    view_manager_ = view_manager;
    root_ = root;
    view_manager_->SetWindowManagerDelegate(this);

    window_container_ = mojo::view_manager::Node::Create(view_manager_);
    window_container_->SetBounds(root_->bounds());
    root_->AddChild(window_container_);

  }
  virtual void OnViewManagerDisconnected(
      mojo::view_manager::ViewManager* view_manager) MOJO_OVERRIDE {
    view_manager_ = NULL;
    root_ = NULL;
  }

  // Overridden from mojo::view_manager::WindowManagerDelegate:
  virtual void Embed(const mojo::String& url) MOJO_OVERRIDE {
    mojo::view_manager::Node* embed_node =
        mojo::view_manager::Node::Create(view_manager_);
    embed_node->SetBounds(gfx::Rect(next_window_origin_, gfx::Size(400, 400)));
    window_container_->AddChild(embed_node);
    embed_node->Embed(url);
    next_window_origin_.Offset(50, 50);
  }
  virtual void DispatchEvent(mojo::view_manager::View* target,
                             mojo::EventPtr event) MOJO_OVERRIDE {
    view_manager_->DispatchEvent(target, event.Pass());
  }

  scoped_ptr<mojo::WindowManagerApp> window_manager_app_;

  mojo::view_manager::ViewManager* view_manager_;
  mojo::view_manager::Node* root_;
  mojo::view_manager::Node* window_container_;

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
