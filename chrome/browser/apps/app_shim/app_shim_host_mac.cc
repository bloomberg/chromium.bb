// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_shim/app_shim_handler_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/ns_view_bridge_factory_host.h"
#include "content/public/common/ns_view_bridge_factory.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/cocoa/bridge_factory_host.h"
#include "ui/views_bridge_mac/mojo/bridge_factory.mojom.h"

namespace {
// Start counting host ids at 1000 to help in debugging.
uint64_t g_next_host_id = 1000;
}  // namespace

AppShimHost::AppShimHost(const std::string& app_id,
                         const base::FilePath& profile_path)
    : host_binding_(this),
      app_shim_request_(mojo::MakeRequest(&app_shim_)),
      app_id_(app_id),
      profile_path_(profile_path) {
  // Create the interfaces used to host windows, so that browser windows may be
  // created before the host process finishes launching.
  if (features::HostWindowsInAppShimProcess()) {
    uint64_t host_id = g_next_host_id++;

    // Create the interface that will be used by views::NativeWidgetMac to
    // create NSWindows hosted in the app shim process.
    views_bridge_mac::mojom::BridgeFactoryAssociatedRequest
        views_bridge_factory_request;
    views_bridge_factory_host_ = std::make_unique<views::BridgeFactoryHost>(
        host_id, &views_bridge_factory_request);
    app_shim_->CreateViewsBridgeFactory(
        std::move(views_bridge_factory_request));

    // Create the interface that will be used content::RenderWidgetHostView to
    // create NSViews hosted in the app shim process.
    content::mojom::NSViewBridgeFactoryAssociatedRequest
        content_bridge_factory_request;
    content_bridge_factory_ =
        std::make_unique<content::NSViewBridgeFactoryHost>(
            &content_bridge_factory_request, host_id);
    app_shim_->CreateContentNSViewBridgeFactory(
        std::move(content_bridge_factory_request));
  }
}

AppShimHost::~AppShimHost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Ensure that we send a response to the bootstrap even if we failed to finish
  // loading.
  if (bootstrap_ && !has_sent_on_launch_complete_)
    bootstrap_->OnLaunchAppFailed(apps::APP_SHIM_LAUNCH_APP_NOT_FOUND);
}

void AppShimHost::ChannelError(uint32_t custom_reason,
                               const std::string& description) {
  LOG(ERROR) << "Channel error custom_reason:" << custom_reason
             << " description: " << description;
  Close();
}

void AppShimHost::SendLaunchResult() {
  DCHECK(!has_sent_on_launch_complete_);
  DCHECK(bootstrap_);
  DCHECK(launch_result_);
  if (*launch_result_ == apps::APP_SHIM_LAUNCH_SUCCESS)
    bootstrap_->OnLaunchAppSucceeded(std::move(app_shim_request_));
  else
    bootstrap_->OnLaunchAppFailed(*launch_result_);
  has_sent_on_launch_complete_ = true;
}

void AppShimHost::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Note that we must call GetAppShimHandler here and not in the destructor
  // because some tests override the method.
  apps::AppShimHandler* handler = GetAppShimHandler();
  if (handler)
    handler->OnShimClose(this);
  delete this;
}

apps::AppShimHandler* AppShimHost::GetAppShimHandler() const {
  return apps::AppShimHandler::GetForAppMode(app_id_);
}

////////////////////////////////////////////////////////////////////////////////
// AppShimHost, chrome::mojom::AppShimHost

bool AppShimHost::HasBootstrapConnected() const {
  return bootstrap_ != nullptr;
}

void AppShimHost::OnBootstrapConnected(
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  DCHECK(!bootstrap_);
  bootstrap_ = std::move(bootstrap);

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  host_binding_.Bind(bootstrap_->GetLaunchAppShimHostRequest());
  host_binding_.set_connection_error_with_reason_handler(
      base::BindOnce(&AppShimHost::ChannelError, base::Unretained(this)));

  // If we already have a launch result ready (e.g, because we launched the app
  // from Chrome), send the result immediately.
  if (launch_result_)
    SendLaunchResult();
}

void AppShimHost::FocusApp(apps::AppShimFocusType focus_type,
                           const std::vector<base::FilePath>& files) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  apps::AppShimHandler* handler = GetAppShimHandler();
  if (handler)
    handler->OnShimFocus(this, focus_type, files);
}

void AppShimHost::SetAppHidden(bool hidden) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  apps::AppShimHandler* handler = GetAppShimHandler();
  if (handler)
    handler->OnShimSetHidden(this, hidden);
}

void AppShimHost::QuitApp() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  apps::AppShimHandler* handler = GetAppShimHandler();
  if (handler)
    handler->OnShimQuit(this);
}

////////////////////////////////////////////////////////////////////////////////
// AppShimHost, apps::AppShimHandler::Host

void AppShimHost::OnAppLaunchComplete(apps::AppShimLaunchResult result) {
  DCHECK(!has_sent_on_launch_complete_);
  launch_result_.emplace(result);
  if (bootstrap_)
    SendLaunchResult();
}

void AppShimHost::OnAppClosed() {
  Close();
}

void AppShimHost::OnAppHide() {
  app_shim_->Hide();
}

void AppShimHost::OnAppUnhideWithoutActivation() {
  app_shim_->UnhideWithoutActivation();
}

void AppShimHost::OnAppRequestUserAttention(apps::AppShimAttentionType type) {
  app_shim_->SetUserAttention(type);
}

base::FilePath AppShimHost::GetProfilePath() const {
  return profile_path_;
}

std::string AppShimHost::GetAppId() const {
  return app_id_;
}

views::BridgeFactoryHost* AppShimHost::GetViewsBridgeFactoryHost() const {
  return views_bridge_factory_host_.get();
}
