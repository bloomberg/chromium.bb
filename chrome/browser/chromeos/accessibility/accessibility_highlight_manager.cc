// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_highlight_manager.h"

#include "ash/accessibility/accessibility_focus_ring_controller.h"
#include "ash/shell.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/public/activation_client.h"

using ash::AccessibilityFocusRingController;

namespace chromeos {

namespace {

ui::InputMethod* GetInputMethod(aura::Window* root_window) {
  if (root_window->GetHost())
    return root_window->GetHost()->GetInputMethod();
  return nullptr;
}

}  // namespace

AccessibilityHighlightManager::AccessibilityHighlightManager() {}

AccessibilityHighlightManager::~AccessibilityHighlightManager() {
  // No need to do anything during shutdown
  if (!ash::Shell::HasInstance())
    return;

  AccessibilityFocusRingController::GetInstance()->SetFocusRing(
      std::vector<gfx::Rect>(),
      AccessibilityFocusRingController::FADE_OUT_FOCUS_RING);
  AccessibilityFocusRingController::GetInstance()->HideCaretRing();
  AccessibilityFocusRingController::GetInstance()->HideCursorRing();

  ash::Shell* shell = ash::Shell::Get();
  if (shell && registered_observers_) {
    shell->RemovePreTargetHandler(this);
    shell->cursor_manager()->RemoveObserver(this);

    aura::Window* root_window = shell->GetPrimaryRootWindow();
    ui::InputMethod* input_method = GetInputMethod(root_window);
    input_method->RemoveObserver(this);
  }
}

void AccessibilityHighlightManager::HighlightFocus(bool focus) {
  focus_ = focus;
  UpdateFocusAndCaretHighlights();
}

void AccessibilityHighlightManager::HighlightCursor(bool cursor) {
  cursor_ = cursor;
  UpdateCursorHighlight();
}

void AccessibilityHighlightManager::HighlightCaret(bool caret) {
  caret_ = caret;
  UpdateFocusAndCaretHighlights();
}

void AccessibilityHighlightManager::RegisterObservers() {
  ash::Shell* shell = ash::Shell::Get();
  shell->AddPreTargetHandler(this);
  shell->cursor_manager()->AddObserver(this);
  registrar_.Add(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                 content::NotificationService::AllSources());
  aura::Window* root_window = ash::Shell::GetPrimaryRootWindow();
  ui::InputMethod* input_method = GetInputMethod(root_window);
  input_method->AddObserver(this);
  registered_observers_ = true;
}

void AccessibilityHighlightManager::OnViewFocusedInArc(
    const gfx::Rect& bounds_in_screen) {
  focus_rect_ = bounds_in_screen;
  UpdateFocusAndCaretHighlights();
}

void AccessibilityHighlightManager::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_MOVED) {
    cursor_point_ = event->location();
    if (event->target()) {
      ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event->target()),
                                 &cursor_point_);
    }
    UpdateCursorHighlight();
  }
}

void AccessibilityHighlightManager::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() == ui::ET_KEY_PRESSED)
    UpdateFocusAndCaretHighlights();
}

void AccessibilityHighlightManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE);
  content::FocusedNodeDetails* node_details =
      content::Details<content::FocusedNodeDetails>(details).ptr();
  focus_rect_ = node_details->node_bounds_in_screen;
  UpdateFocusAndCaretHighlights();
}

void AccessibilityHighlightManager::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  if (!client || client->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE) {
    caret_visible_ = false;
    UpdateFocusAndCaretHighlights();
  }
}

void AccessibilityHighlightManager::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
  if (!client || client->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE) {
    caret_visible_ = false;
    return;
  }
  gfx::Rect caret_bounds = client->GetCaretBounds();
  gfx::Point new_caret_point = caret_bounds.CenterPoint();
  ::wm::ConvertPointFromScreen(ash::Shell::GetPrimaryRootWindow(),
                               &new_caret_point);
  if (new_caret_point == caret_point_)
    return;
  caret_point_ = new_caret_point;
  caret_visible_ = IsCaretVisible(caret_bounds);
  UpdateFocusAndCaretHighlights();
}

void AccessibilityHighlightManager::OnCursorVisibilityChanged(bool is_visible) {
  UpdateCursorHighlight();
}

bool AccessibilityHighlightManager::IsCursorVisible() {
  return ash::Shell::Get()->cursor_manager()->IsCursorVisible();
}

bool AccessibilityHighlightManager::IsCaretVisible(
    const gfx::Rect& caret_bounds) {
  aura::Window* root_window = ash::Shell::GetPrimaryRootWindow();
  aura::Window* active_window =
      ::wm::GetActivationClient(root_window)->GetActiveWindow();
  if (!active_window)
    active_window = root_window;
  return (caret_bounds.width() || caret_bounds.height()) &&
         active_window->GetBoundsInScreen().Contains(caret_point_);
}

void AccessibilityHighlightManager::UpdateFocusAndCaretHighlights() {
  auto* controller = AccessibilityFocusRingController::GetInstance();

  // The caret highlight takes precedence over the focus highlight if
  // both are visible.
  if (caret_ && caret_visible_) {
    controller->SetCaretRing(caret_point_);
    controller->SetFocusRing(
        std::vector<gfx::Rect>(),
        AccessibilityFocusRingController::FADE_OUT_FOCUS_RING);
  } else if (focus_) {
    controller->HideCaretRing();
    std::vector<gfx::Rect> rects;
    if (!focus_rect_.IsEmpty())
      rects.push_back(focus_rect_);
    controller->SetFocusRing(
        rects, AccessibilityFocusRingController::FADE_OUT_FOCUS_RING);
  } else {
    controller->HideCaretRing();
    controller->SetFocusRing(
        std::vector<gfx::Rect>(),
        AccessibilityFocusRingController::FADE_OUT_FOCUS_RING);
  }
}

void AccessibilityHighlightManager::UpdateCursorHighlight() {
  if (cursor_ && IsCursorVisible()) {
    AccessibilityFocusRingController::GetInstance()->SetCursorRing(
        cursor_point_);
  } else {
    AccessibilityFocusRingController::GetInstance()->HideCursorRing();
  }
}

}  // namespace chromeos
