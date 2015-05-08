// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/browser/browser.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "mandoline/ui/browser/browser_ui.h"
#include "mandoline/ui/browser/merged_service_provider.h"
#include "mojo/application/application_runner_chromium.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"
#include "ui/gfx/geometry/size.h"

namespace mandoline {

Browser::Browser()
    : window_manager_app_(new window_manager::WindowManagerApp(this, this)),
      root_(nullptr),
      content_(nullptr),
      navigator_host_(this),
      ui_(nullptr),
      weak_factory_(this) {
  exposed_services_impl_.AddService(this);
}

Browser::~Browser() {
}

base::WeakPtr<Browser> Browser::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void Browser::Initialize(mojo::ApplicationImpl* app) {
  window_manager_app_->Initialize(app);
  ui_.reset(BrowserUI::Create(this, app->shell()));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringVector args = command_line->GetArgs();
  if (args.empty()) {
    default_url_ = "http://www.google.com/";
  } else {
#if defined(OS_WIN)
    default_url_ = base::WideToUTF8(args[0]);
#else
    default_url_ = args[0];
#endif
  }
}

bool Browser::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  window_manager_app_->ConfigureIncomingConnection(connection);
  return true;
}

bool Browser::ConfigureOutgoingConnection(
    mojo::ApplicationConnection* connection) {
  window_manager_app_->ConfigureOutgoingConnection(connection);
  return true;
}

void Browser::OnEmbed(
    mojo::View* root,
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr exposed_services) {
  // Browser does not support being embedded more than once.
  CHECK(!root_);

  // TODO(beng): still unhappy with the fact that both this class & the UI class
  //             know so much about these views. Figure out how to shift more to
  //             the UI class.
  root_ = root;
  content_ = root->view_manager()->CreateView();
  ui_->Init(root_, content_);

#if defined(OS_ANDROID)
  // Resize to match the Nexus 5 aspect ratio:
  window_manager_app_->SetViewportSize(gfx::Size(320, 640));
#else
  window_manager_app_->SetViewportSize(gfx::Size(1280, 800));
#endif

  root_->AddChild(content_);
  content_->SetVisible(true);

  window_manager_app_->AddAccelerator(mojo::KEYBOARD_CODE_BROWSER_BACK,
                                      mojo::EVENT_FLAGS_NONE);

  // Now that we're ready, either load a pending url or the default url.
  if (!pending_url_.empty())
    Embed(pending_url_, services.Pass(), exposed_services.Pass());
  else if (!default_url_.empty())
    Embed(default_url_, services.Pass(), exposed_services.Pass());
}

void Browser::Embed(const mojo::String& url,
                    mojo::InterfaceRequest<mojo::ServiceProvider> services,
                    mojo::ServiceProviderPtr exposed_services) {
  // We can get Embed calls before we've actually been
  // embedded into the root view and content_ is created.
  // Just save the last url, we'll embed it when we're ready.
  if (!content_) {
    pending_url_ = url;
    return;
  }

  merged_service_provider_.reset(
      new MergedServiceProvider(exposed_services.Pass(), this));
  content_->Embed(url, services.Pass(),
                  merged_service_provider_->GetServiceProviderPtr().Pass());

  navigator_host_.RecordNavigation(url);
}

void Browser::OnAcceleratorPressed(mojo::View* view,
                                   mojo::KeyboardCode keyboard_code,
                                   mojo::EventFlags flags) {
  DCHECK_EQ(mojo::KEYBOARD_CODE_BROWSER_BACK, keyboard_code);
  navigator_host_.RequestNavigateHistory(-1);
}

void Browser::Create(mojo::ApplicationConnection* connection,
                     mojo::InterfaceRequest<mojo::NavigatorHost> request) {
  navigator_host_.Bind(request.Pass());
}

void Browser::OnViewManagerDisconnected(
    mojo::ViewManager* view_manager) {
  root_ = nullptr;
}

// Convenience method:
void Browser::ReplaceContentWithURL(const mojo::String& url) {
  Embed(url, nullptr, nullptr);
}

}  // namespace mandoline
