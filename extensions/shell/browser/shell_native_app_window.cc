// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_native_app_window.h"

#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

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
  NOTIMPLEMENTED();
  return false;
}

bool ShellNativeAppWindow::IsMaximized() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellNativeAppWindow::IsMinimized() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellNativeAppWindow::IsFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

gfx::NativeWindow ShellNativeAppWindow::GetNativeWindow() {
  return GetWindow();
}

gfx::Rect ShellNativeAppWindow::GetRestoredBounds() const {
  NOTIMPLEMENTED();
  return GetBounds();
}

ui::WindowShowState ShellNativeAppWindow::GetRestoredState() const {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::Activate() {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::Deactivate() {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
  return false;
}

void ShellNativeAppWindow::UpdateWindowIcon() {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::UpdateWindowTitle() {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::UpdateBadgeIcon() {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
}

bool ShellNativeAppWindow::IsFrameless() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellNativeAppWindow::HasFrameColor() const {
  NOTIMPLEMENTED();
  return false;
}

SkColor ShellNativeAppWindow::ActiveFrameColor() const {
  NOTIMPLEMENTED();
  return SkColor();
}

SkColor ShellNativeAppWindow::InactiveFrameColor() const {
  NOTIMPLEMENTED();
  return SkColor();
}

gfx::Insets ShellNativeAppWindow::GetFrameInsets() const {
  NOTIMPLEMENTED();
  return gfx::Insets();
}

void ShellNativeAppWindow::ShowWithApp() {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::HideWithApp() {
  NOTIMPLEMENTED();
}

void ShellNativeAppWindow::UpdateShelfMenu() {
  NOTIMPLEMENTED();
}

gfx::Size ShellNativeAppWindow::GetContentMinimumSize() const {
  NOTIMPLEMENTED();
  return gfx::Size();
}

gfx::Size ShellNativeAppWindow::GetContentMaximumSize() const {
  NOTIMPLEMENTED();
  return gfx::Size();
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
  NOTIMPLEMENTED();
  return false;
}

aura::Window* ShellNativeAppWindow::GetWindow() const {
  return app_window_->web_contents()->GetNativeView();
}

}  // namespace extensions
