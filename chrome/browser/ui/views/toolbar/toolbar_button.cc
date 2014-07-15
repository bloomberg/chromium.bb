// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

#include "base/bind.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "grit/theme_resources.h"
#include "grit/ui_strings.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

ToolbarButton::ToolbarButton(views::ButtonListener* listener,
                             ui::MenuModel* model)
    : views::LabelButton(listener, base::string16()),
      model_(model),
      menu_showing_(false),
      y_position_on_lbuttondown_(0),
      show_menu_factory_(this) {
  set_context_menu_controller(this);
}

ToolbarButton::~ToolbarButton() {
}

void ToolbarButton::Init() {
  SetFocusable(false);
  SetAccessibilityFocusable(true);
  image()->EnableCanvasFlippingForRTLUI(true);
}

void ToolbarButton::ClearPendingMenu() {
  show_menu_factory_.InvalidateWeakPtrs();
}

bool ToolbarButton::IsMenuShowing() const {
  return menu_showing_;
}

gfx::Size ToolbarButton::GetPreferredSize() const {
  gfx::Size size(image()->GetPreferredSize());
  gfx::Size label_size = label()->GetPreferredSize();
  if (label_size.width() > 0)
    size.Enlarge(label_size.width() + LocationBarView::kItemPadding, 0);
  return size;
}

bool ToolbarButton::OnMousePressed(const ui::MouseEvent& event) {
  if (enabled() && ShouldShowMenu() &&
      IsTriggerableEvent(event) && HitTestPoint(event.location())) {
    // Store the y pos of the mouse coordinates so we can use them later to
    // determine if the user dragged the mouse down (which should pop up the
    // drag down menu immediately, instead of waiting for the timer)
    y_position_on_lbuttondown_ = event.y();

    // Schedule a task that will show the menu.
    const int kMenuTimerDelay = 500;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ToolbarButton::ShowDropDownMenu,
                   show_menu_factory_.GetWeakPtr(),
                   ui::GetMenuSourceTypeForEvent(event)),
        base::TimeDelta::FromMilliseconds(kMenuTimerDelay));
  }
  return LabelButton::OnMousePressed(event);
}

bool ToolbarButton::OnMouseDragged(const ui::MouseEvent& event) {
  bool result = LabelButton::OnMouseDragged(event);

  if (show_menu_factory_.HasWeakPtrs()) {
    // If the mouse is dragged to a y position lower than where it was when
    // clicked then we should not wait for the menu to appear but show
    // it immediately.
    if (event.y() > y_position_on_lbuttondown_ + GetHorizontalDragThreshold()) {
      show_menu_factory_.InvalidateWeakPtrs();
      ShowDropDownMenu(ui::GetMenuSourceTypeForEvent(event));
    }
  }

  return result;
}

void ToolbarButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (IsTriggerableEvent(event) ||
      (event.IsRightMouseButton() && !HitTestPoint(event.location()))) {
    LabelButton::OnMouseReleased(event);
  }

  if (IsTriggerableEvent(event))
    show_menu_factory_.InvalidateWeakPtrs();
}

void ToolbarButton::OnMouseCaptureLost() {
}

void ToolbarButton::OnMouseExited(const ui::MouseEvent& event) {
  // Starting a drag results in a MouseExited, we need to ignore it.
  // A right click release triggers an exit event. We want to
  // remain in a PUSHED state until the drop down menu closes.
  if (state_ != STATE_DISABLED && !InDrag() && state_ != STATE_PRESSED)
    SetState(STATE_NORMAL);
}

void ToolbarButton::OnGestureEvent(ui::GestureEvent* event) {
  if (menu_showing_) {
    // While dropdown menu is showing the button should not handle gestures.
    event->StopPropagation();
    return;
  }

  LabelButton::OnGestureEvent(event);
}

void ToolbarButton::GetAccessibleState(ui::AXViewState* state) {
  CustomButton::GetAccessibleState(state);
  state->role = ui::AX_ROLE_BUTTON_DROP_DOWN;
  state->default_action = l10n_util::GetStringUTF16(IDS_APP_ACCACTION_PRESS);
  state->AddStateFlag(ui::AX_STATE_HASPOPUP);
}

scoped_ptr<views::LabelButtonBorder>
ToolbarButton::CreateDefaultBorder() const {
  scoped_ptr<views::LabelButtonBorder> border =
      LabelButton::CreateDefaultBorder();

  ui::ThemeProvider* provider = GetThemeProvider();
  if (provider && provider->UsingSystemTheme()) {
    // We set smaller insets here to accommodate the slightly larger GTK+
    // icons.
    border->set_insets(gfx::Insets(2, 2, 2, 2));
  }

  return border.Pass();
}

void ToolbarButton::ShowContextMenuForView(View* source,
                                           const gfx::Point& point,
                                           ui::MenuSourceType source_type) {
  if (!enabled())
    return;

  show_menu_factory_.InvalidateWeakPtrs();
  ShowDropDownMenu(source_type);
}

bool ToolbarButton::ShouldEnterPushedState(const ui::Event& event) {
  // Enter PUSHED state on press with Left or Right mouse button or on taps.
  // Remain in this state while the context menu is open.
  return event.type() == ui::ET_GESTURE_TAP ||
         event.type() == ui::ET_GESTURE_TAP_DOWN ||
         (event.IsMouseEvent() && ((ui::EF_LEFT_MOUSE_BUTTON |
             ui::EF_RIGHT_MOUSE_BUTTON) & event.flags()) != 0);
}

bool ToolbarButton::ShouldShowMenu() {
  return model_ != NULL;
}

void ToolbarButton::ShowDropDownMenu(ui::MenuSourceType source_type) {
  if (!ShouldShowMenu())
    return;

  gfx::Rect lb = GetLocalBounds();

  // Both the menu position and the menu anchor type change if the UI layout
  // is right-to-left.
  gfx::Point menu_position(lb.origin());
  menu_position.Offset(0, lb.height() - 1);
  if (base::i18n::IsRTL())
    menu_position.Offset(lb.width() - 1, 0);

  View::ConvertPointToScreen(this, &menu_position);

#if defined(OS_WIN)
  int left_bound = GetSystemMetrics(SM_XVIRTUALSCREEN);
#elif defined(OS_CHROMEOS)
  // A window won't overlap between displays on ChromeOS.
  // Use the left bound of the display on which
  // the menu button exists.
  gfx::NativeView view = GetWidget()->GetNativeView();
  gfx::Display display = gfx::Screen::GetScreenFor(
      view)->GetDisplayNearestWindow(view);
  int left_bound = display.bounds().x();
#else
  // The window might be positioned over the edge between two screens. We'll
  // want to position the dropdown on the screen the mouse cursor is on.
  gfx::NativeView view = GetWidget()->GetNativeView();
  gfx::Screen* screen = gfx::Screen::GetScreenFor(view);
  gfx::Display display = screen->GetDisplayNearestPoint(
      screen->GetCursorScreenPoint());
  int left_bound = display.bounds().x();
#endif
  if (menu_position.x() < left_bound)
    menu_position.set_x(left_bound);

  // Make the button look depressed while the menu is open.
  SetState(STATE_PRESSED);

  menu_showing_ = true;

  // Create and run menu.  Display an empty menu if model is NULL.
  if (model_.get()) {
    views::MenuModelAdapter menu_delegate(model_.get());
    menu_delegate.set_triggerable_event_flags(triggerable_event_flags());
    menu_runner_.reset(new views::MenuRunner(menu_delegate.CreateMenu(),
                                             views::MenuRunner::HAS_MNEMONICS));
    views::MenuRunner::RunResult result =
        menu_runner_->RunMenuAt(GetWidget(),
                                NULL,
                                gfx::Rect(menu_position, gfx::Size(0, 0)),
                                views::MENU_ANCHOR_TOPLEFT,
                                source_type);
    if (result == views::MenuRunner::MENU_DELETED)
      return;
  } else {
    views::MenuDelegate menu_delegate;
    views::MenuItemView* menu = new views::MenuItemView(&menu_delegate);
    menu_runner_.reset(
        new views::MenuRunner(menu, views::MenuRunner::HAS_MNEMONICS));
    views::MenuRunner::RunResult result =
        menu_runner_->RunMenuAt(GetWidget(),
                                NULL,
                                gfx::Rect(menu_position, gfx::Size(0, 0)),
                                views::MENU_ANCHOR_TOPLEFT,
                                source_type);
    if (result == views::MenuRunner::MENU_DELETED)
      return;
  }

  menu_showing_ = false;

  // Need to explicitly clear mouse handler so that events get sent
  // properly after the menu finishes running. If we don't do this, then
  // the first click to other parts of the UI is eaten.
  SetMouseHandler(NULL);

  // Set the state back to normal after the drop down menu is closed.
  if (state_ != STATE_DISABLED)
    SetState(STATE_NORMAL);
}
