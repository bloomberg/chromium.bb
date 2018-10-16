// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils_aura.h"

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"

namespace {

class FocusWaiter : public aura::client::FocusChangeObserver {
 public:
  explicit FocusWaiter(aura::Window* window) : window_(window) {
    aura::client::GetFocusClient(window_)->AddObserver(this);
  }

  ~FocusWaiter() override {
    aura::client::GetFocusClient(window_)->RemoveObserver(this);
  }

  void WaitUntilFocus() {
    if (window_->HasFocus())
      return;
    run_loop_.Run();
  }

 private:
  // aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override {
    if (gained_focus == window_)
      run_loop_.QuitWhenIdle();
  }

  aura::Window* window_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(FocusWaiter);
};

}  // namespace

namespace ui_test_utils {

#if !defined(OS_WIN)
void HideNativeWindow(gfx::NativeWindow window) {
  HideNativeWindowAura(window);
}

bool ShowAndFocusNativeWindow(gfx::NativeWindow window) {
  return ShowAndFocusNativeWindowAura(window);
}
#endif

void HideNativeWindowAura(gfx::NativeWindow window) {
  window->Hide();
}

bool ShowAndFocusNativeWindowAura(gfx::NativeWindow window) {
  window->Show();
  window->Focus();
  return true;
}

void WaitUntilWindowFocused(aura::Window* window) {
  FocusWaiter waiter(window);
  waiter.WaitUntilFocus();
}

}  // namespace ui_test_utils
