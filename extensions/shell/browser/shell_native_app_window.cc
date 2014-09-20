// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_native_app_window.h"

#include "content/public/browser/web_contents.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/wm/core/window_util.h"

namespace extensions {
namespace {

gfx::Size GetDesktopWindowSize() {
  return DesktopController::instance()->GetHost()->window()->bounds().size();
}

}  // namespace

ShellNativeAppWindow::ShellNativeAppWindow(
    AppWindow* app_window,
    const AppWindow::CreateParams& params)
    : app_window_(app_window) {
  gfx::Rect bounds = params.GetInitialWindowBounds(GetFrameInsets());
  bool position_specified =
      bounds.x() != AppWindow::BoundsSpecification::kUnspecifiedPosition &&
      bounds.y() != AppWindow::BoundsSpecification::kUnspecifiedPosition;
  if (!position_specified)
    bounds.set_origin(GetBounds().origin());
  SetBounds(bounds);
}

ShellNativeAppWindow::~ShellNativeAppWindow() {
}

bool ShellNativeAppWindow::IsActive() const {
  // Even though app_shell only supports a single app window, there might be
  // some sort of system-level dialog open and active.
  aura::Window* window = GetWindow();
  return window && wm::IsActiveWindow(window);
}

bool ShellNativeAppWindow::IsMaximized() const {
  return false;
}

bool ShellNativeAppWindow::IsMinimized() const {
  return false;
}

bool ShellNativeAppWindow::IsFullscreen() const {
  // The window in app_shell is considered a "restored" window that happens to
  // fill the display. This avoids special handling of fullscreen or maximized
  // windows that app_shell doesn't need.
  return false;
}

gfx::NativeWindow ShellNativeAppWindow::GetNativeWindow() {
  return GetWindow();
}

gfx::Rect ShellNativeAppWindow::GetRestoredBounds() const {
  // app_shell windows cannot be maximized, so the current bounds are the
  // restored bounds.
  return GetBounds();
}

ui::WindowShowState ShellNativeAppWindow::GetRestoredState() const {
  return ui::SHOW_STATE_NORMAL;
}

gfx::Rect ShellNativeAppWindow::GetBounds() const {
  return GetWindow()->GetBoundsInScreen();
}

void ShellNativeAppWindow::Show() {
  GetWindow()->Show();
}

void ShellNativeAppWindow::Hide() {
  GetWindow()->Hide();
}

void ShellNativeAppWindow::ShowInactive() {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::Close() {
  app_window_->OnNativeClose();
}

void ShellNativeAppWindow::Activate() {
  aura::Window* window = GetWindow();
  if (window)
    wm::ActivateWindow(window);
}

void ShellNativeAppWindow::Deactivate() {
  aura::Window* window = GetWindow();
  if (window)
    wm::DeactivateWindow(window);
}

void ShellNativeAppWindow::Maximize() {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::Minimize() {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::Restore() {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::SetBounds(const gfx::Rect& bounds) {
  GetWindow()->SetBounds(bounds);
}

void ShellNativeAppWindow::FlashFrame(bool flash) {
  NOTIMPLEMENTED();
}

bool ShellNativeAppWindow::IsAlwaysOnTop() const {
  return false;
}

void ShellNativeAppWindow::SetAlwaysOnTop(bool always_on_top) {
  NOTIMPLEMENTED();
}

gfx::NativeView ShellNativeAppWindow::GetHostView() const {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::Point ShellNativeAppWindow::GetDialogPosition(const gfx::Size& size) {
  NOTIMPLEMENTED();
  return gfx::Point();
}

void ShellNativeAppWindow::AddObserver(
      web_modal::ModalDialogHostObserver* observer) {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::RemoveObserver(
      web_modal::ModalDialogHostObserver* observer) {
  NOTIMPLEMENTED();
}

gfx::Size ShellNativeAppWindow::GetMaximumDialogSize() {
  NOTIMPLEMENTED();
  return gfx::Size();
}

void ShellNativeAppWindow::SetFullscreen(int fullscreen_types) {
  NOTIMPLEMENTED();
}

bool ShellNativeAppWindow::IsFullscreenOrPending() const {
  // See comment in IsFullscreen().
  return false;
}

void ShellNativeAppWindow::UpdateWindowIcon() {
  // No icon to update.
}

void ShellNativeAppWindow::UpdateWindowTitle() {
  // No window title to update.
}

void ShellNativeAppWindow::UpdateBadgeIcon() {
  // No badge to update.
}

void ShellNativeAppWindow::UpdateDraggableRegions(
    const std::vector<DraggableRegion>& regions) {
  NOTIMPLEMENTED();
}

SkRegion* ShellNativeAppWindow::GetDraggableRegion() {
  NOTIMPLEMENTED();
  return NULL;
}

void ShellNativeAppWindow::UpdateShape(scoped_ptr<SkRegion> region) {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) {
  // No special handling. The WebContents will handle it.
}

bool ShellNativeAppWindow::IsFrameless() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellNativeAppWindow::HasFrameColor() const {
  return false;
}

SkColor ShellNativeAppWindow::ActiveFrameColor() const {
  return SkColor();
}

SkColor ShellNativeAppWindow::InactiveFrameColor() const {
  return SkColor();
}

gfx::Insets ShellNativeAppWindow::GetFrameInsets() const {
  return gfx::Insets();
}

void ShellNativeAppWindow::ShowWithApp() {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::HideWithApp() {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::UpdateShelfMenu() {
  // app_shell has no shelf, dock, or system-tray to update.
}

gfx::Size ShellNativeAppWindow::GetContentMinimumSize() const {
  // Content fills the desktop and cannot be resized.
  return GetDesktopWindowSize();
}

gfx::Size ShellNativeAppWindow::GetContentMaximumSize() const {
  // Content fills the desktop and cannot be resized.
  return GetDesktopWindowSize();
}

void ShellNativeAppWindow::SetContentSizeConstraints(
    const gfx::Size& min_size,
    const gfx::Size& max_size) {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::SetVisibleOnAllWorkspaces(bool always_visible) {
  NOTIMPLEMENTED();
}

bool ShellNativeAppWindow::CanHaveAlphaEnabled() const {
  // No background to display if the window was transparent.
  return false;
}

aura::Window* ShellNativeAppWindow::GetWindow() const {
  return app_window_->web_contents()->GetNativeView();
}

}  // namespace extensions
