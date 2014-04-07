// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_desktop_controller.h"

#include "apps/shell/browser/shell_app_window.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/gfx/screen.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/test/wm_test_helper.h"

#if defined(OS_CHROMEOS)
#include "ui/display/chromeos/display_mode.h"
#include "ui/display/chromeos/display_snapshot.h"
#endif

namespace apps {
namespace {

const SkColor kBackgroundColor = SK_ColorBLACK;

// A simple layout manager that makes each new window fill its parent.
class FillLayout : public aura::LayoutManager {
 public:
  FillLayout() {}
  virtual ~FillLayout() {}

 private:
  // aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE {}

  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {
    if (!child->parent())
      return;

    // Create a rect at 0,0 with the size of the parent.
    gfx::Size parent_size = child->parent()->bounds().size();
    child->SetBounds(gfx::Rect(parent_size));
  }

  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {}

  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {}

  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {}

  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {
    SetChildBoundsDirect(child, requested_bounds);
  }

  DISALLOW_COPY_AND_ASSIGN(FillLayout);
};

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

ShellDesktopController* g_instance = NULL;

}  // namespace

ShellDesktopController::ShellDesktopController() {
#if defined(OS_CHROMEOS)
  display_configurator_.reset(new ui::DisplayConfigurator);
  display_configurator_->Init(false);
  display_configurator_->ForceInitialConfigure(0);
  display_configurator_->AddObserver(this);
#endif
  CreateRootWindow();

  DCHECK(!views::ViewsDelegate::views_delegate);
  views::ViewsDelegate::views_delegate =
      new ShellViewsDelegate(wm_test_helper_->host()->window());

  g_instance = this;
}

ShellDesktopController::~ShellDesktopController() {
  // The app window must be explicitly closed before desktop teardown.
  DCHECK(!app_window_);
  g_instance = NULL;
  delete views::ViewsDelegate::views_delegate;
  views::ViewsDelegate::views_delegate = NULL;
  DestroyRootWindow();
  aura::Env::DeleteInstance();
}

// static
ShellDesktopController* ShellDesktopController::instance() {
  return g_instance;
}

ShellAppWindow* ShellDesktopController::CreateAppWindow(
    content::BrowserContext* context) {
  aura::Window* root_window = GetWindowTreeHost()->window();

  app_window_.reset(new ShellAppWindow);
  app_window_->Init(context, root_window->bounds().size());

  // Attach the web contents view to our window hierarchy.
  aura::Window* content = app_window_->GetNativeWindow();
  root_window->AddChild(content);
  content->Show();

  return app_window_.get();
}

void ShellDesktopController::CloseAppWindow() { app_window_.reset(); }

aura::WindowTreeHost* ShellDesktopController::GetWindowTreeHost() {
  return wm_test_helper_->host();
}

#if defined(OS_CHROMEOS)
void ShellDesktopController::OnDisplayModeChanged(
    const std::vector<ui::DisplayConfigurator::DisplayState>& outputs) {
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
  aura::WindowTreeHost* host = wm_test_helper_->host();
  host->compositor()->SetBackgroundColor(kBackgroundColor);

  // Ensure new windows fill the display.
  host->window()->SetLayoutManager(new FillLayout);

  // Ensure the X window gets mapped.
  host->Show();
}

void ShellDesktopController::DestroyRootWindow() {
  wm_test_helper_.reset();
  ui::ShutdownInputMethodForTesting();
}

gfx::Size ShellDesktopController::GetPrimaryDisplaySize() {
#if defined(OS_CHROMEOS)
  const std::vector<ui::DisplayConfigurator::DisplayState>& states =
      display_configurator_->cached_outputs();
  if (states.empty())
    return gfx::Size();
  const ui::DisplayMode* mode = states[0].display->current_mode();
  return mode ? mode->size() : gfx::Size();
#else
  return gfx::Size();
#endif
}

}  // namespace apps
