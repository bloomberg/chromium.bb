// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_AURA_H_
#define CONTENT_SHELL_BROWSER_SHELL_AURA_H_

#include "base/memory/scoped_ptr.h"

namespace aura {
namespace client {
class DefaultActivationClient;
class DefaultCaptureClient;
class FocusClient;
class WindowTreeClient;
}
class RootWindow;
}

namespace ui {
class EventHandler;
}

namespace content {

class ShellAuraPlatformData {
 public:
  ShellAuraPlatformData();
  ~ShellAuraPlatformData();

  void ResizeWindow(int width, int height);

  aura::RootWindow* window() { return root_window_.get(); }

 private:
  scoped_ptr<aura::RootWindow> root_window_;
  scoped_ptr<aura::client::FocusClient> focus_client_;
  scoped_ptr<aura::client::DefaultActivationClient> activation_client_;
  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;
  scoped_ptr<aura::client::WindowTreeClient> window_tree_client_;
  scoped_ptr<ui::EventHandler> ime_filter_;

  DISALLOW_COPY_AND_ASSIGN(ShellAuraPlatformData);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_AURA_H_
