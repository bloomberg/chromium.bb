// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/kiosk_wm/kiosk_wm.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "components/kiosk_wm/merged_service_provider.h"
#include "ui/gfx/geometry/size.h"

namespace kiosk_wm {

KioskWM::KioskWM()
    : window_manager_app_(new window_manager::WindowManagerApp(this, this)),
      root_(nullptr),
      content_(nullptr),
      navigator_host_(this),
      weak_factory_(this) {
  exposed_services_impl_.AddService(this);
}

KioskWM::~KioskWM() {
}

base::WeakPtr<KioskWM> KioskWM::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void KioskWM::Initialize(mojo::ApplicationImpl* app) {
  window_manager_app_->Initialize(app);

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

bool KioskWM::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  window_manager_app_->ConfigureIncomingConnection(connection);
  return true;
}

bool KioskWM::ConfigureOutgoingConnection(
    mojo::ApplicationConnection* connection) {
  window_manager_app_->ConfigureOutgoingConnection(connection);
  return true;
}

void KioskWM::OnEmbed(
    mojo::View* root,
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr exposed_services) {
  // KioskWM does not support being embedded more than once.
  CHECK(!root_);

  root_ = root;
  root_->AddObserver(this);

#if defined(OS_ANDROID)
  // Resize to match the Nexus 5 aspect ratio:
  window_manager_app_->SetViewportSize(gfx::Size(320, 640));
#else
  window_manager_app_->SetViewportSize(gfx::Size(1280, 800));
#endif

  content_ = root->view_manager()->CreateView();
  content_->SetBounds(root_->bounds());
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

void KioskWM::Embed(const mojo::String& url,
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

void KioskWM::OnAcceleratorPressed(mojo::View* view,
                                   mojo::KeyboardCode keyboard_code,
                                   mojo::EventFlags flags) {
  DCHECK_EQ(mojo::KEYBOARD_CODE_BROWSER_BACK, keyboard_code);
  navigator_host_.RequestNavigateHistory(-1);
}

void KioskWM::Create(mojo::ApplicationConnection* connection,
                     mojo::InterfaceRequest<mojo::NavigatorHost> request) {
  navigator_host_.Bind(request.Pass());
}

void KioskWM::OnViewManagerDisconnected(
    mojo::ViewManager* view_manager) {
  root_ = nullptr;
}

void KioskWM::OnViewDestroyed(mojo::View* view) {
  view->RemoveObserver(this);
}

void KioskWM::OnViewBoundsChanged(mojo::View* view,
                                              const mojo::Rect& old_bounds,
                                              const mojo::Rect& new_bounds) {
  content_->SetBounds(new_bounds);
}

// Convenience method:
void KioskWM::ReplaceContentWithURL(const mojo::String& url) {
  Embed(url, nullptr, nullptr);
}

}  // namespace kiosk_wm
