// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/browser/browser.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager_init.h"
#include "mandoline/ui/browser/browser_delegate.h"
#include "mandoline/ui/browser/browser_ui.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"
#include "ui/gfx/geometry/size.h"

namespace mandoline {
namespace {

gfx::Size GetInitialViewportSize() {
#if defined(OS_ANDROID)
  // Resize to match the Nexus 5 aspect ratio:
  return gfx::Size(320, 640);
#else
  return gfx::Size(1280, 800);
#endif
}

}  // namespace

Browser::Browser(mojo::ApplicationImpl* app,
                 BrowserDelegate* delegate,
                 const GURL& default_url)
    : view_manager_init_(app, this, this),
      root_(nullptr),
      content_(nullptr),
      default_url_(default_url),
      navigator_host_(this),
      web_view_(this),
      app_(app),
      delegate_(delegate) {
  ui_.reset(BrowserUI::Create(this, app));
}

Browser::~Browser() {
  // Destruct ui_ manually while |this| is alive and reset the pointer first.
  // This is to avoid a double delete when OnViewManagerDestroyed gets
  // called.
  ui_.reset();
}

void Browser::ReplaceContentWithRequest(mojo::URLRequestPtr request) {
  Embed(request.Pass());
}

void Browser::ShowOmnibox() {
  if (!omnibox_.get()) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From("mojo:omnibox");
    omnibox_connection_ = app_->ConnectToApplication(request.Pass());
    omnibox_connection_->AddService<ViewEmbedder>(this);
    omnibox_connection_->ConnectToService(&omnibox_);
    omnibox_connection_->SetRemoteServiceProviderConnectionErrorHandler(
        [this]() {
          // This will cause the connection to be re-established the next time
          // we come through this codepath.
          omnibox_.reset();
        });
  }
  omnibox_->ShowForURL(mojo::String::From(current_url_.spec()));
}

mojo::ApplicationConnection* Browser::GetViewManagerConnectionForTesting() {
  return view_manager_init_.connection();
}

void Browser::OnEmbed(mojo::View* root) {
  // Browser does not support being embedded more than once.
  CHECK(!root_);

  // Make it so we get OnWillEmbed() for any Embed()s done by other apps we
  // Embed().
  root->view_manager()->SetEmbedRoot();

  // TODO(beng): still unhappy with the fact that both this class & the UI class
  //             know so much about these views. Figure out how to shift more to
  //             the UI class.
  root_ = root;

  delegate_->InitUIIfNecessary(this, root_);

  content_ = root_->view_manager()->CreateView();
  ui_->Init(root_);

  view_manager_init_.view_manager_root()->SetViewportSize(
      mojo::Size::From(GetInitialViewportSize()));

  root_->AddChild(content_);
  content_->SetVisible(true);

  web_view_.Init(app_, content_);

  view_manager_init_.view_manager_root()->AddAccelerator(
      mojo::KEYBOARD_CODE_BROWSER_BACK, mojo::EVENT_FLAGS_NONE);

  // Now that we're ready, load the default url.
  if (default_url_.is_valid()) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From(default_url_.spec());
    Embed(request.Pass());
  }
}

void Browser::OnViewManagerDestroyed(mojo::ViewManager* view_manager) {
  ui_.reset();
  root_ = nullptr;
  delegate_->BrowserClosed(this);
}

void Browser::OnAccelerator(mojo::EventPtr event) {
  DCHECK_EQ(mojo::KEYBOARD_CODE_BROWSER_BACK,
            event->key_data->windows_key_code);
  navigator_host_.RequestNavigateHistory(-1);
}

void Browser::TopLevelNavigate(mojo::URLRequestPtr request) {
  Embed(request.Pass());
}

void Browser::LoadingStateChanged(bool is_loading) {
  ui_->LoadingStateChanged(is_loading);
}

void Browser::ProgressChanged(double progress) {
  ui_->ProgressChanged(progress);
}

// TODO(beng): Consider moving this to the UI object as well once the frame tree
//             stuff is better encapsulated.
void Browser::Embed(mojo::URLRequestPtr request) {
  const std::string string_url = request->url.To<std::string>();
  if (string_url == "mojo:omnibox") {
    ui_->EmbedOmnibox(omnibox_connection_.get());
    return;
  }

  GURL gurl(string_url);
  bool changed = current_url_ != gurl;
  current_url_ = gurl;
  if (changed)
    ui_->OnURLChanged();

  web_view_.web_view()->LoadRequest(request.Pass());

  navigator_host_.RecordNavigation(gurl.spec());
}

void Browser::Create(mojo::ApplicationConnection* connection,
                     mojo::InterfaceRequest<mojo::NavigatorHost> request) {
  navigator_host_.Bind(request.Pass());
}

void Browser::Create(mojo::ApplicationConnection* connection,
                     mojo::InterfaceRequest<ViewEmbedder> request) {
  view_embedder_bindings_.AddBinding(this, request.Pass());
}

}  // namespace mandoline
