// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/phone_ui/phone_browser_application_delegate.h"

#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_tree_connection.h"
#include "components/view_manager/public/cpp/view_tree_host_factory.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
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
  mojo::CreateSingleViewTreeHost(app_, this, &host_);
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
// PhoneBrowserApplicationDelegate, mojo::ViewTreeDelegate implementation:

void PhoneBrowserApplicationDelegate::OnEmbed(mojo::View* root) {
  root->connection()->SetEmbedRoot();
  content_ = root->connection()->CreateView();
  root->AddChild(content_);
  content_->SetBounds(root->bounds());
  content_->SetVisible(true);

  host_->SetSize(mojo::Size::From(gfx::Size(320, 640)));
  web_view_.Init(app_, content_);
  LaunchURL("http://www.google.com/");
}

void PhoneBrowserApplicationDelegate::OnConnectionLost(
    mojo::ViewTreeConnection* connection) {
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
