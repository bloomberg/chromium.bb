// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/frame/touch_browser_frame_view.h"

#include "chrome/browser/ui/touch/keyboard/keyboard_manager.h"
#include "ui/base/ime/text_input_type.h"
#include "views/controls/button/image_button.h"
#include "views/controls/textfield/textfield.h"
#include "views/focus/focus_manager.h"
#include "views/ime/text_input_client.h"
#include "views/window/hit_test.h"

// static
const char TouchBrowserFrameView::kViewClassName[] =
    "browser/ui/touch/frame/TouchBrowserFrameView";

///////////////////////////////////////////////////////////////////////////////
// TouchBrowserFrameView, public:

TouchBrowserFrameView::TouchBrowserFrameView(BrowserFrame* frame,
                                             BrowserView* browser_view)
    : OpaqueBrowserFrameView(frame, browser_view),
      focus_listener_added_(false) {
  // Make sure the singleton KeyboardManager object is initialized.
  KeyboardManager::GetInstance();
}

TouchBrowserFrameView::~TouchBrowserFrameView() {
}

///////////////////////////////////////////////////////////////////////////////
// TouchBrowserFrameView, private:

void TouchBrowserFrameView::FocusWillChange(views::View* focused_before,
                                            views::View* focused_now) {
  views::Widget* widget = focused_now ? focused_now->GetWidget() : NULL;
  if (!widget || !widget->IsActive())
    return;

  views::TextInputClient* input =
      focused_now ? focused_now->GetTextInputClient() : NULL;
  // Show the keyboard if the focused view supports text-input.
  KeyboardManager* keyboard = KeyboardManager::GetInstance();
  if (input && input->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE)
    keyboard->ShowKeyboardForWidget(focused_now->GetWidget());
  else
    keyboard->Hide();
}

std::string TouchBrowserFrameView::GetClassName() const {
  return kViewClassName;
}

void TouchBrowserFrameView::ViewHierarchyChanged(bool is_add,
                                                 View* parent,
                                                 View* child) {
  OpaqueBrowserFrameView::ViewHierarchyChanged(is_add, parent, child);
  if (!GetFocusManager())
    return;

  if (is_add && !focus_listener_added_) {
    // Add focus listener when this view is added to the hierarchy.
    GetFocusManager()->AddFocusChangeListener(this);
    focus_listener_added_ = true;
  } else if (!is_add && focus_listener_added_) {
    // Remove focus listener when this view is removed from the hierarchy.
    GetFocusManager()->RemoveFocusChangeListener(this);
    focus_listener_added_ = false;
  }
}

bool TouchBrowserFrameView::HitTest(const gfx::Point& point) const {
  if (OpaqueBrowserFrameView::HitTest(point))
    return true;

  if (close_button()->IsVisible() &&
      close_button()->GetMirroredBounds().Contains(point))
    return true;
  if (restore_button()->IsVisible() &&
      restore_button()->GetMirroredBounds().Contains(point))
    return true;
  if (maximize_button()->IsVisible() &&
      maximize_button()->GetMirroredBounds().Contains(point))
    return true;
  if (minimize_button()->IsVisible() &&
      minimize_button()->GetMirroredBounds().Contains(point))
    return true;

  return false;
}
