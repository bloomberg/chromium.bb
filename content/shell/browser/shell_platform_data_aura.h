// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_PLATFORM_DATA_AURA_H_
#define CONTENT_SHELL_BROWSER_SHELL_PLATFORM_DATA_AURA_H_

#include "base/memory/scoped_ptr.h"

namespace aura {
class WindowEventDispatcher;
namespace client {
class DefaultActivationClient;
class DefaultCaptureClient;
class FocusClient;
class WindowTreeClient;
}
}

namespace gfx {
class Size;
}

namespace ui {
class EventHandler;
}

namespace content {

class ShellPlatformDataAura {
 public:
  explicit ShellPlatformDataAura(const gfx::Size& initial_size);
  ~ShellPlatformDataAura();

  void ShowWindow();
  void ResizeWindow(const gfx::Size& size);

  aura::WindowEventDispatcher* dispatcher() { return dispatcher_.get(); }

 private:
  scoped_ptr<aura::WindowEventDispatcher> dispatcher_;
  scoped_ptr<aura::client::FocusClient> focus_client_;
  scoped_ptr<aura::client::DefaultActivationClient> activation_client_;
  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;
  scoped_ptr<aura::client::WindowTreeClient> window_tree_client_;
  scoped_ptr<ui::EventHandler> ime_filter_;

  DISALLOW_COPY_AND_ASSIGN(ShellPlatformDataAura);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_PLATFORM_DATA_AURA_H_
