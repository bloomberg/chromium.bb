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
      profile_path_(profile_path),
      weak_factory_(this) {
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
}

void AppShimHost::ChannelError(uint32_t custom_reason,
                               const std::string& description) {
  LOG(ERROR) << "Channel error custom_reason:" << custom_reason
             << " description: " << description;
  Close();
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

void AppShimHost::OnShimLaunchCompleted(bool recreate_shims_requested,
                                        base::Process shim_process) {
  // If the shim process was created, assume that it will successfully create
  // an AppShimHostBootstrap.
  // TODO(https://crbug.com/913362): We should add a callback for when
  // |shim_process| exits, and handle the case where that happens without the
  // creation of an AppShimHostBootstrap.
  if (shim_process.IsValid())
    return;

  // If this was a launch without recreating shims, then the launch may have
  // failed because the shims were not present, or because they were out of
  // date. Try again, recreating the shims this time.
  if (!recreate_shims_requested) {
    DLOG(ERROR) << "Failed to launch shim, attempting to recreate.";
    constexpr bool recreate_shims = true;
    apps::AppShimHandler* handler = GetAppShimHandler();
    handler->OnShimLaunchRequested(
        this, recreate_shims,
        base::BindOnce(&AppShimHost::OnShimLaunchCompleted,
                       weak_factory_.GetWeakPtr(), recreate_shims));
    return;
  }

  // If we attempted to recreate the app shims and still failed to launch, then
  // there is no hope to launch the app. Close its windows (since they will
  // never be seen).
  // TODO(https://crbug.com/913362): Consider adding some UI to tell the
  // user that the process launch failed.
  DLOG(ERROR) << "Failed to launch recreated shim, giving up.";
  OnAppClosed();
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
  bootstrap_->OnConnectedToHost(std::move(app_shim_request_));

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  host_binding_.Bind(bootstrap_->GetLaunchAppShimHostRequest());
  host_binding_.set_connection_error_with_reason_handler(
      base::BindOnce(&AppShimHost::ChannelError, base::Unretained(this)));
}

void AppShimHost::LaunchShim() {
  apps::AppShimHandler* handler = GetAppShimHandler();
  if (!handler)
    return;
  if (bootstrap_) {
    // If there is a connected app shim process, focus the app windows.
    handler->OnShimFocus(this, apps::APP_SHIM_FOCUS_NORMAL,
                         std::vector<base::FilePath>());
  } else {
    // Otherwise, attempt to launch whatever app shims we find.
    constexpr bool recreate_shims = false;
    handler->OnShimLaunchRequested(
        this, recreate_shims,
        base::BindOnce(&AppShimHost::OnShimLaunchCompleted,
                       weak_factory_.GetWeakPtr(), recreate_shims));
  }
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

chrome::mojom::AppShim* AppShimHost::GetAppShim() const {
  return app_shim_.get();
}
