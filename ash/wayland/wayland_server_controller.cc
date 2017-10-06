// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wayland/wayland_server_controller.h"

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/config.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "components/exo/display.h"
#include "components/exo/file_helper.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wm_helper_ash.h"

namespace ash {

class WaylandServerController::WaylandWatcher
    : public base::MessagePumpLibevent::Watcher {
 public:
  explicit WaylandWatcher(exo::wayland::Server* server)
      : controller_(FROM_HERE), server_(server) {
    base::MessageLoopForUI::current()->WatchFileDescriptor(
        server_->GetFileDescriptor(),
        true,  // persistent
        base::MessagePumpLibevent::WATCH_READ, &controller_, this);
  }

  // base::MessagePumpLibevent::Watcher:
  void OnFileCanReadWithoutBlocking(int fd) override {
    server_->Dispatch(base::TimeDelta());
    server_->Flush();
  }
  void OnFileCanWriteWithoutBlocking(int fd) override { NOTREACHED(); }

 private:
  base::MessagePumpLibevent::FileDescriptorWatcher controller_;
  exo::wayland::Server* const server_;

  DISALLOW_COPY_AND_ASSIGN(WaylandWatcher);
};

// static
std::unique_ptr<WaylandServerController>
WaylandServerController::CreateIfNecessary() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableWaylandServer)) {
    return nullptr;
  }

  return base::WrapUnique(new WaylandServerController());
}

WaylandServerController::~WaylandServerController() {
  wayland_watcher_.reset();
  wayland_server_.reset();
  exo::WMHelper::SetInstance(nullptr);
  wm_helper_.reset();
}

WaylandServerController::WaylandServerController() {
  wm_helper_ = std::make_unique<exo::WMHelperAsh>();
  exo::WMHelper::SetInstance(wm_helper_.get());
  // TODO(penghuang): wire up notification surface manager.
  // http://crbug.com/768439
  // TODO(hirono): wire up the file helper. http://crbug.com/768395
  display_ = std::make_unique<exo::Display>(
      nullptr /* notification_surface_manager */, nullptr /* file_helper */);
  wayland_server_ = exo::wayland::Server::Create(display_.get());
  // Wayland server creation can fail if XDG_RUNTIME_DIR is not set correctly.
  if (wayland_server_)
    wayland_watcher_ = std::make_unique<WaylandWatcher>(wayland_server_.get());
}

}  // namespace ash
