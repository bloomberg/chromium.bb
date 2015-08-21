// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/phone_ui/phone_browser_application_delegate.h"

#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "components/view_manager/public/cpp/view_manager_init.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace mandoline {

////////////////////////////////////////////////////////////////////////////////
// PhoneBrowserApplicationDelegate, public:

PhoneBrowserApplicationDelegate::PhoneBrowserApplicationDelegate()
    : app_(nullptr),
      content_(nullptr),
      web_view_(this) {
}

PhoneBrowserApplicationDelegate::~PhoneBrowserApplicationDelegate() {
}

////////////////////////////////////////////////////////////////////////////////
// PhoneBrowserApplicationDelegate, mojo::ApplicationDelegate implementation:

void PhoneBrowserApplicationDelegate::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;

  // This results in this application being embedded at the root view.
  // Initialization continues below in OnEmbed()...
  init_.reset(new mojo::ViewManagerInit(app, this, nullptr));
}

bool PhoneBrowserApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<LaunchHandler>(this);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// PhoneBrowserApplicationDelegate, LaunchHandler implementation:

void PhoneBrowserApplicationDelegate::LaunchURL(const mojo::String& url) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = url;
  web_view_.web_view()->LoadRequest(request.Pass());
}

////////////////////////////////////////////////////////////////////////////////
// PhoneBrowserApplicationDelegate, mojo::ViewManagerDelegate implementation:

void PhoneBrowserApplicationDelegate::OnEmbed(mojo::View* root) {
  root->view_manager()->SetEmbedRoot();
  content_ = root->view_manager()->CreateView();
  root->AddChild(content_);
  content_->SetBounds(root->bounds());
  content_->SetVisible(true);

  init_->view_manager_root()->SetViewportSize(
      mojo::Size::From(gfx::Size(320, 640)));
  web_view_.Init(app_, content_);
  LaunchURL("http://www.google.com/");
}

void PhoneBrowserApplicationDelegate::OnViewManagerDestroyed(
    mojo::ViewManager* view_manager) {
}

////////////////////////////////////////////////////////////////////////////////
// PhoneBrowserApplicationDelegate, mojo::ViewObserver implementation:

void PhoneBrowserApplicationDelegate::OnViewBoundsChanged(
    mojo::View* view,
    const mojo::Rect& old_bounds,
    const mojo::Rect& new_bounds) {
  content_->SetBounds(
      *mojo::Rect::From(gfx::Rect(0, 0, new_bounds.width, new_bounds.height)));
}

////////////////////////////////////////////////////////////////////////////////
// PhoneBrowserApplicationDelegate,
//     web_view::mojom::WebViewClient implementation:

void PhoneBrowserApplicationDelegate::TopLevelNavigate(
    mojo::URLRequestPtr request) {
  web_view_.web_view()->LoadRequest(request.Pass());
}

void PhoneBrowserApplicationDelegate::LoadingStateChanged(bool is_loading) {
  // ...
}

void PhoneBrowserApplicationDelegate::ProgressChanged(double progress) {
  // ...
}

////////////////////////////////////////////////////////////////////////////////
// PhoneBrowserApplicationDelegate,
//       mojo::InterfaceFactory<LaunchHandler> implementation:

void PhoneBrowserApplicationDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<LaunchHandler> request) {
  launch_handler_bindings_.AddBinding(this, request.Pass());
}

}  // namespace mandoline
