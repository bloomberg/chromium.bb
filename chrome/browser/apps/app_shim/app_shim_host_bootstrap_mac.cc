// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "mojo/public/cpp/system/message_pipe.h"

// static
void AppShimHostBootstrap::CreateForChannel(
    mojo::PlatformChannelEndpoint endpoint) {
  // AppShimHostBootstrap is initially owned by itself until it receives a
  // LaunchApp message or a channel error. In LaunchApp, ownership is
  // transferred to a unique_ptr.
  (new AppShimHostBootstrap)->ServeChannel(std::move(endpoint));
}

AppShimHostBootstrap::AppShimHostBootstrap() : host_bootstrap_binding_(this) {}

AppShimHostBootstrap::~AppShimHostBootstrap() {
  DCHECK(!launch_app_callback_);
}

void AppShimHostBootstrap::ServeChannel(
    mojo::PlatformChannelEndpoint endpoint) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  mojo::ScopedMessagePipeHandle message_pipe =
      bootstrap_mojo_connection_.Connect(std::move(endpoint));
  host_bootstrap_binding_.Bind(
      chrome::mojom::AppShimHostBootstrapRequest(std::move(message_pipe)));
  host_bootstrap_binding_.set_connection_error_with_reason_handler(
      base::BindOnce(&AppShimHostBootstrap::ChannelError,
                     base::Unretained(this)));
}

void AppShimHostBootstrap::ChannelError(uint32_t custom_reason,
                                        const std::string& description) {
  // Once |this| has received a LaunchApp message, it is owned by a unique_ptr
  // (not the channel anymore).
  if (has_received_launch_app_)
    return;
  LOG(ERROR) << "Channel error custom_reason:" << custom_reason
             << " description: " << description;
  delete this;
}

apps::AppShimHandler* AppShimHostBootstrap::GetHandler() {
  return apps::AppShimHandler::GetForAppMode(app_id_);
}

chrome::mojom::AppShimHostRequest
AppShimHostBootstrap::GetLaunchAppShimHostRequest() {
  return std::move(app_shim_host_request_);
}

void AppShimHostBootstrap::LaunchApp(
    chrome::mojom::AppShimHostRequest app_shim_host_request,
    const base::FilePath& profile_dir,
    const std::string& app_id,
    apps::AppShimLaunchType launch_type,
    const std::vector<base::FilePath>& files,
    LaunchAppCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!has_received_launch_app_);
  // Only one app launch message per channel.
  if (has_received_launch_app_)
    return;

  app_shim_host_request_ = std::move(app_shim_host_request);
  profile_path_ = profile_dir;
  app_id_ = app_id;
  launch_type_ = launch_type;
  files_ = files;
  launch_app_callback_ = std::move(callback);

  // Transfer ownership to a unique_ptr and mark that LaunchApp has been
  // received. Note that after this point, a channel error will no longer
  // cause |this| to be deleted.
  has_received_launch_app_ = true;
  std::unique_ptr<AppShimHostBootstrap> deleter(this);

  // |handler| takes ownership of |this| now.
  apps::AppShimHandler* handler = GetHandler();
  if (handler)
    handler->OnShimLaunch(std::move(deleter));
  // |handler| can only be NULL after AppShimHostManager is destroyed. Since
  // this only happens at shutdown, do nothing here.
}

void AppShimHostBootstrap::OnLaunchAppSucceeded(
    chrome::mojom::AppShimRequest app_shim_request) {
  std::move(launch_app_callback_)
      .Run(apps::APP_SHIM_LAUNCH_SUCCESS, std::move(app_shim_request));
}

void AppShimHostBootstrap::OnLaunchAppFailed(apps::AppShimLaunchResult result) {
  // Because there will be users of the AppShim interface in failure, just
  // return a dummy request.
  chrome::mojom::AppShimPtr dummy_ptr;
  std::move(launch_app_callback_).Run(result, mojo::MakeRequest(&dummy_ptr));
}
