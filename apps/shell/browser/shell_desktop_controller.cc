// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_desktop_controller.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/gfx/screen.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/test/wm_test_helper.h"

#if defined(OS_CHROMEOS) && defined(USE_X11)
#include "ui/display/chromeos/display_mode.h"
#include "ui/display/chromeos/display_snapshot.h"
#endif

namespace apps {
namespace {

const SkColor kBackgroundColor = SK_ColorBLACK;

// A ViewsDelegate to attach new unparented windows to app_shell's root window.
class ShellViewsDelegate : public views::TestViewsDelegate {
 public:
  explicit ShellViewsDelegate(aura::Window* root_window)
      : root_window_(root_window) {}
  virtual ~ShellViewsDelegate() {}

  // views::ViewsDelegate implementation.
  virtual void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) OVERRIDE {
    if (!params->parent)
      params->parent = root_window_;
  }

 private:
  aura::Window* root_window_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(ShellViewsDelegate);
};

}  // namespace

ShellDesktopController::ShellDesktopController() {
#if defined(OS_CHROMEOS) && defined(USE_X11)
  output_configurator_.reset(new ui::OutputConfigurator);
  output_configurator_->Init(false);
  output_configurator_->ForceInitialConfigure(0);
  output_configurator_->AddObserver(this);
#endif
  CreateRootWindow();

  DCHECK(!views::ViewsDelegate::views_delegate);
  views::ViewsDelegate::views_delegate =
      new ShellViewsDelegate(wm_test_helper_->host()->window());
}

ShellDesktopController::~ShellDesktopController() {
  delete views::ViewsDelegate::views_delegate;
  views::ViewsDelegate::views_delegate = NULL;
  DestroyRootWindow();
  aura::Env::DeleteInstance();
}

aura::WindowTreeHost* ShellDesktopController::GetWindowTreeHost() {
  return wm_test_helper_->host();
}

#if defined(OS_CHROMEOS) && defined(USE_X11)
void ShellDesktopController::OnDisplayModeChanged(
    const std::vector<ui::OutputConfigurator::DisplayState>& outputs) {
  gfx::Size size = GetPrimaryDisplaySize();
  if (!size.IsEmpty())
    wm_test_helper_->host()->UpdateRootWindowSize(size);
}
#endif

void ShellDesktopController::CreateRootWindow() {
  test_screen_.reset(aura::TestScreen::Create());
  // TODO(jamescook): Replace this with a real Screen implementation.
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, test_screen_.get());
  // TODO(jamescook): Initialize a real input method.
  ui::InitializeInputMethodForTesting();

  // Set up basic pieces of views::corewm.
  gfx::Size size = GetPrimaryDisplaySize();
  if (size.IsEmpty())
    size = gfx::Size(800, 600);
  wm_test_helper_.reset(new wm::WMTestHelper(size));
  wm_test_helper_->host()->compositor()->SetBackgroundColor(kBackgroundColor);

  // Ensure the X window gets mapped.
  wm_test_helper_->host()->Show();
}

void ShellDesktopController::DestroyRootWindow() {
  wm_test_helper_.reset();
  ui::ShutdownInputMethodForTesting();
}

gfx::Size ShellDesktopController::GetPrimaryDisplaySize() {
#if defined(OS_CHROMEOS) && defined(USE_X11)
  const std::vector<ui::OutputConfigurator::DisplayState>& states =
      output_configurator_->cached_outputs();
  if (states.empty())
    return gfx::Size();
  const ui::DisplayMode* mode = states[0].display->current_mode();
  return mode ? mode->size() : gfx::Size();
#else
  return gfx::Size();
#endif
}

}  // namespace apps
