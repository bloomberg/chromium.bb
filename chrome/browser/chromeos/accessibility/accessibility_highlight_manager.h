// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_HIGHLIGHT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_HIGHLIGHT_MANAGER_H_

#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos {

// Manage visual highlights that Chrome OS can draw around the focused
// object, the cursor, and the text caret for accessibility.
class AccessibilityHighlightManager
    : public ui::EventHandler,
      public content::NotificationObserver,
      public ui::InputMethodObserver,
      public aura::client::CursorClientObserver {
 public:
  AccessibilityHighlightManager();
  ~AccessibilityHighlightManager() override;

  void HighlightFocus(bool focus);
  void HighlightCursor(bool cursor);
  void HighlightCaret(bool caret);

  void RegisterObservers();

  // OnViewFocusedInArc is called when a view is focused in arc window and
  // accessibility focus highlight is enabled.
  void OnViewFocusedInArc(const gfx::Rect& bounds_in_screen);

 protected:
  FRIEND_TEST_ALL_PREFIXES(AccessibilityFocusRingControllerTest,
                           CursorWorksOnMultipleDisplays);

  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ui::InputMethodObserver overrides:
  void OnTextInputTypeChanged(const ui::TextInputClient* client) override {}
  void OnFocus() override {}
  void OnBlur() override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}
  void OnShowImeIfNeeded() override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;

  // aura::client::CursorClientObserver
  void OnCursorVisibilityChanged(bool is_visible) override;

  virtual bool IsCursorVisible();

 private:
  void UpdateFocusAndCaretHighlights();
  void UpdateCursorHighlight();

  bool focus_ = false;
  gfx::Rect focus_rect_;

  bool cursor_ = false;
  gfx::Point cursor_point_;

  bool caret_ = false;
  bool caret_visible_ = false;
  gfx::Point caret_point_;

  bool registered_observers_ = false;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityHighlightManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_HIGHLIGHT_MANAGER_H_
