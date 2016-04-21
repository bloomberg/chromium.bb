// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/button_ink_drop_delegate.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

ToolbarButton::ToolbarButton(Profile* profile,
                             views::ButtonListener* listener,
                             ui::MenuModel* model)
    : views::LabelButton(listener, base::string16()),
      profile_(profile),
      model_(model),
      menu_showing_(false),
      y_position_on_lbuttondown_(0),
      ink_drop_delegate_(new views::ButtonInkDropDelegate(this, this)),
      show_menu_factory_(this) {
  set_ink_drop_delegate(ink_drop_delegate_.get());
  set_has_ink_drop_action_on_click(true);
  set_context_menu_controller(this);
}

ToolbarButton::~ToolbarButton() {}

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
  if (label_size.width() > 0) {
    size.Enlarge(
        label_size.width() + GetLayoutConstant(LOCATION_BAR_HORIZONTAL_PADDING),
        0);
  }
  // For non-material assets the entire size of the button is captured in the
  // image resource. For Material Design the excess whitespace is being removed
  // from the image assets. Enlarge the button by the theme provided insets.
  if (ui::MaterialDesignController::IsModeMaterial()) {
    const ui::ThemeProvider* provider = GetThemeProvider();
    if (provider) {
      gfx::Insets insets(GetLayoutInsets(TOOLBAR_BUTTON));
      size.Enlarge(insets.width(), insets.height());
    }
  }
  return size;
}

bool ToolbarButton::OnMousePressed(const ui::MouseEvent& event) {
  if (IsTriggerableEvent(event) && enabled() && ShouldShowMenu() &&
      HitTestPoint(event.location())) {
    // Store the y pos of the mouse coordinates so we can use them later to
    // determine if the user dragged the mouse down (which should pop up the
    // drag down menu immediately, instead of waiting for the timer)
    y_position_on_lbuttondown_ = event.y();

    // Schedule a task that will show the menu.
    const int kMenuTimerDelay = 500;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&ToolbarButton::ShowDropDownMenu,
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
  if (state() != STATE_DISABLED && !InDrag() && state() != STATE_PRESSED)
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

std::unique_ptr<views::LabelButtonBorder> ToolbarButton::CreateDefaultBorder()
    const {
  std::unique_ptr<views::LabelButtonBorder> border =
      views::LabelButton::CreateDefaultBorder();

  if (ThemeServiceFactory::GetForProfile(profile_)->UsingSystemTheme())
    border->set_insets(GetLayoutInsets(TOOLBAR_BUTTON));

  return border;
}

void ToolbarButton::ShowContextMenuForView(View* source,
                                           const gfx::Point& point,
                                           ui::MenuSourceType source_type) {
  if (!enabled())
    return;

  show_menu_factory_.InvalidateWeakPtrs();
  ShowDropDownMenu(source_type);
}

bool ToolbarButton::ShouldShowMenu() {
  return model_ != nullptr;
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
  gfx::Display display =
      gfx::Screen::GetScreen()->GetDisplayNearestWindow(view);
  int left_bound = display.bounds().x();
#else
  // The window might be positioned over the edge between two screens. We'll
  // want to position the dropdown on the screen the mouse cursor is on.
  gfx::Screen* screen = gfx::Screen::GetScreen();
  gfx::Display display = screen->GetDisplayNearestPoint(
      screen->GetCursorScreenPoint());
  int left_bound = display.bounds().x();
#endif
  if (menu_position.x() < left_bound)
    menu_position.set_x(left_bound);

  // Make the button look depressed while the menu is open.
  SetState(STATE_PRESSED);

  menu_showing_ = true;

  ink_drop_delegate()->OnAction(views::InkDropState::ACTIVATED);

  // Exit if the model is null.
  if (!model_.get())
    return;

  // Create and run menu.
  menu_model_adapter_.reset(new views::MenuModelAdapter(
      model_.get(),
      base::Bind(&ToolbarButton::OnMenuClosed, base::Unretained(this))));
  menu_model_adapter_->set_triggerable_event_flags(triggerable_event_flags());
  menu_runner_.reset(new views::MenuRunner(
      menu_model_adapter_->CreateMenu(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::ASYNC));
  ignore_result(menu_runner_->RunMenuAt(
      GetWidget(), nullptr, gfx::Rect(menu_position, gfx::Size(0, 0)),
      views::MENU_ANCHOR_TOPLEFT, source_type));
}

void ToolbarButton::OnMenuClosed() {
  ink_drop_delegate()->OnAction(views::InkDropState::DEACTIVATED);

  menu_showing_ = false;

  // Need to explicitly clear mouse handler so that events get sent
  // properly after the menu finishes running. If we don't do this, then
  // the first click to other parts of the UI is eaten.
  SetMouseHandler(nullptr);

  // Set the state back to normal after the drop down menu is closed.
  if (state() != STATE_DISABLED)
    SetState(STATE_NORMAL);

  menu_runner_.reset();
  menu_model_adapter_.reset();
}

const char* ToolbarButton::GetClassName() const {
  return "ToolbarButton";
}
