// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_exo.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/common/chrome_switches.h"
#include "components/exo/display.h"
#include "components/exo/wayland/server.h"
#include "content/public/browser/browser_thread.h"

class ChromeBrowserMainExtraPartsExo::WaylandWatcher
    : public base::MessagePumpLibevent::Watcher {
 public:
  explicit WaylandWatcher(exo::wayland::Server* server) : server_(server) {
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

ChromeBrowserMainExtraPartsExo::ChromeBrowserMainExtraPartsExo()
    : display_(new exo::Display) {}

ChromeBrowserMainExtraPartsExo::~ChromeBrowserMainExtraPartsExo() {}

void ChromeBrowserMainExtraPartsExo::PreProfileInit() {
  if (!chrome::ShouldOpenAshOnStartup())
    return;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWaylandServer)) {
    wayland_server_ = exo::wayland::Server::Create(display_.get());
    wayland_watcher_ =
        make_scoped_ptr(new WaylandWatcher(wayland_server_.get()));
  }
}

void ChromeBrowserMainExtraPartsExo::PostMainMessageLoopRun() {
  wayland_watcher_.reset();
  wayland_server_.reset();
}
