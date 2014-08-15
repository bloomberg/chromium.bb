// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/mode_indicator_controller.h"

#include "ash/ime/mode_indicator_view.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"

namespace chromeos {
namespace input_method {

namespace {
ModeIndicatorObserverInterface* g_mode_indicator_observer_for_testing_ = NULL;
}  // namespace

class ModeIndicatorObserver : public ModeIndicatorObserverInterface {
 public:
  ModeIndicatorObserver()
    : active_widget_(NULL) {}

  virtual ~ModeIndicatorObserver() {
    if (active_widget_)
      active_widget_->RemoveObserver(this);
  }

  // If other active mode indicator widget is shown, close it immedicately
  // without fading animation.  Then store this widget as the active widget.
  virtual void AddModeIndicatorWidget(views::Widget* widget) OVERRIDE {
    DCHECK(widget);
    if (active_widget_)
      active_widget_->Close();
    active_widget_ = widget;
    widget->AddObserver(this);
  }

  // views::WidgetObserver override:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE {
    if (widget == active_widget_)
      active_widget_ = NULL;
  }

 private:
  views::Widget* active_widget_;
};


ModeIndicatorController::ModeIndicatorController(InputMethodManager* imm)
    : imm_(imm),
      is_focused_(false),
      mi_observer_(new ModeIndicatorObserver) {
  DCHECK(imm_);
  imm_->AddObserver(this);
}

ModeIndicatorController::~ModeIndicatorController() {
  imm_->RemoveObserver(this);
}

void ModeIndicatorController::SetCursorBounds(
    const gfx::Rect& cursor_bounds) {
  cursor_bounds_ = cursor_bounds;
}

void ModeIndicatorController::FocusStateChanged(bool is_focused) {
  is_focused_ = is_focused;
}

// static
void ModeIndicatorController::SetModeIndicatorObserverForTesting(
    ModeIndicatorObserverInterface* observer) {
  g_mode_indicator_observer_for_testing_ = observer;
}

// static
ModeIndicatorObserverInterface*
ModeIndicatorController::GetModeIndicatorObserverForTesting() {
  return g_mode_indicator_observer_for_testing_;
}

void ModeIndicatorController::InputMethodChanged(InputMethodManager* manager,
                                                 bool show_message) {
  if (!show_message)
    return;
  ShowModeIndicator();
}

void ModeIndicatorController::ShowModeIndicator() {
  // TODO(komatsu): Show the mode indicator in the right bottom of the
  // display when the launch bar is hidden and the focus is out.  To
  // implement it, we should consider to use message center or system
  // notification.  Note, launch bar can be vertical and can be placed
  // right/left side of display.
  if (!is_focused_)
    return;

  // Get the short name of the changed input method (e.g. US, JA, etc.)
  const InputMethodDescriptor descriptor =
      imm_->GetActiveIMEState()->GetCurrentInputMethod();
  const base::string16 short_name =
      imm_->GetInputMethodUtil()->GetInputMethodShortName(descriptor);

  aura::Window* parent =
      ash::Shell::GetContainer(ash::wm::GetActiveWindow()->GetRootWindow(),
                               ash::kShellWindowId_SettingBubbleContainer);
  ash::ime::ModeIndicatorView* mi_view = new ash::ime::ModeIndicatorView(
      parent, cursor_bounds_, short_name);
  views::BubbleDelegateView::CreateBubble(mi_view);

  views::Widget* mi_widget = mi_view->GetWidget();
  if (GetModeIndicatorObserverForTesting())
    GetModeIndicatorObserverForTesting()->AddModeIndicatorWidget(mi_widget);

  mi_observer_->AddModeIndicatorWidget(mi_widget);
  mi_view->ShowAndFadeOut();
}

}  // namespace input_method
}  // namespace chromeos
