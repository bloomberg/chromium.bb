// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/reload_button.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/metrics.h"
#include "ui/views/widget/widget.h"

namespace {

// Contents of the Reload drop-down menu.
const int kReloadMenuItems[]  = {
  IDS_RELOAD_MENU_NORMAL_RELOAD_ITEM,
  IDS_RELOAD_MENU_HARD_RELOAD_ITEM,
  IDS_RELOAD_MENU_EMPTY_AND_HARD_RELOAD_ITEM,
};

const gfx::VectorIcon& GetIconForMode(bool is_reload) {
  if (ui::MaterialDesignController::IsTouchOptimizedUiEnabled())
    return is_reload ? kReloadTouchIcon : kNavigateStopTouchIcon;

  return is_reload ? vector_icons::kReloadIcon : kNavigateStopIcon;
}

}  // namespace

// ReloadButton ---------------------------------------------------------------

// static
const char ReloadButton::kViewClassName[] = "ReloadButton";

ReloadButton::ReloadButton(Profile* profile, CommandUpdater* command_updater)
    : ToolbarButton(profile, this, CreateMenuModel()),
      command_updater_(command_updater),
      double_click_timer_delay_(
          base::TimeDelta::FromMilliseconds(views::GetDoubleClickInterval())),
      mode_switch_timer_delay_(base::TimeDelta::FromMilliseconds(1350)) {}

ReloadButton::~ReloadButton() {}

void ReloadButton::ChangeMode(Mode mode, bool force) {
  intended_mode_ = mode;

  // If the change is forced, or the user isn't hovering the icon, or it's
  // safe to change it to the other image type, make the change immediately;
  // otherwise we'll let it happen later.
  if (force || (!IsMouseHovered() && !testing_mouse_hovered_) ||
      ((mode == Mode::kStop) ? !double_click_timer_.IsRunning()
                             : (visible_mode_ != Mode::kStop))) {
    double_click_timer_.Stop();
    mode_switch_timer_.Stop();
    if (mode != visible_mode_)
      ChangeModeInternal(mode);
    SetEnabled(true);

    // We want to disable the button if we're preventing a change from stop to
    // reload due to hovering, but not if we're preventing a change from
    // reload to stop due to the double-click timer running.  (Disabled reload
    // state is only applicable when instant extended API is enabled and mode
    // is NTP, which is handled just above.)
  } else if (visible_mode_ != Mode::kReload) {
    SetEnabled(false);

    // Go ahead and change to reload after a bit, which allows repeated
    // reloads without moving the mouse.
    if (!mode_switch_timer_.IsRunning()) {
      mode_switch_timer_.Start(FROM_HERE, mode_switch_timer_delay_, this,
                               &ReloadButton::OnStopToReloadTimer);
    }
  }
}

void ReloadButton::LoadImages() {
  ChangeModeInternal(visible_mode_);

  SchedulePaint();
  PreferredSizeChanged();
}

void ReloadButton::OnMouseExited(const ui::MouseEvent& event) {
  ToolbarButton::OnMouseExited(event);
  if (!IsMenuShowing())
    ChangeMode(intended_mode_, true);
}

bool ReloadButton::GetTooltipText(const gfx::Point& p,
                                  base::string16* tooltip) const {
  int reload_tooltip = menu_enabled_ ?
      IDS_TOOLTIP_RELOAD_WITH_MENU : IDS_TOOLTIP_RELOAD;
  int text_id =
      (visible_mode_ == Mode::kReload) ? reload_tooltip : IDS_TOOLTIP_STOP;
  tooltip->assign(l10n_util::GetStringUTF16(text_id));
  return true;
}

const char* ReloadButton::GetClassName() const {
  return kViewClassName;
}

void ReloadButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (menu_enabled_)
    ToolbarButton::GetAccessibleNodeData(node_data);
  else
    Button::GetAccessibleNodeData(node_data);
}

bool ReloadButton::ShouldShowMenu() {
  return menu_enabled_ && (visible_mode_ == Mode::kReload);
}

void ReloadButton::ShowDropDownMenu(ui::MenuSourceType source_type) {
  ToolbarButton::ShowDropDownMenu(source_type);  // Blocks.
  ChangeMode(intended_mode_, true);
}

void ReloadButton::ButtonPressed(views::Button* /* button */,
                                 const ui::Event& event) {
  ClearPendingMenu();

  if (visible_mode_ == Mode::kStop) {
    if (command_updater_)
      command_updater_->ExecuteCommandWithDisposition(
          IDC_STOP, WindowOpenDisposition::CURRENT_TAB);
    // The user has clicked, so we can feel free to update the button, even if
    // the mouse is still hovering.
    ChangeMode(Mode::kReload, true);
    return;
  }

  if (!double_click_timer_.IsRunning()) {
    // Shift-clicking or ctrl-clicking the reload button means we should ignore
    // any cached content.
    int command;
    int flags = event.flags();
    if (event.IsShiftDown() || event.IsControlDown()) {
      command = IDC_RELOAD_BYPASSING_CACHE;
      // Mask off Shift and Control so they don't affect the disposition below.
      flags &= ~(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
    } else {
      command = IDC_RELOAD;
    }

    // Start a timer - while this timer is running, the reload button cannot be
    // changed to a stop button.  We do not set |intended_mode_| to Mode::kStop
    // here as the browser will do that when it actually starts loading (which
    // may happen synchronously, thus the need to do this before telling the
    // browser to execute the reload command).
    double_click_timer_.Start(FROM_HERE, double_click_timer_delay_, this,
                              &ReloadButton::OnDoubleClickTimer);

    ExecuteBrowserCommand(command, flags);
    ++testing_reload_count_;
  }
}

bool ReloadButton::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ReloadButton::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool ReloadButton::IsCommandIdVisible(int command_id) const {
  return true;
}

bool ReloadButton::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  switch (command_id) {
    case IDS_RELOAD_MENU_NORMAL_RELOAD_ITEM:
      GetWidget()->GetAccelerator(IDC_RELOAD, accelerator);
      return true;
    case IDS_RELOAD_MENU_HARD_RELOAD_ITEM:
      GetWidget()->GetAccelerator(IDC_RELOAD_BYPASSING_CACHE, accelerator);
      return true;
  }
  return GetWidget()->GetAccelerator(command_id, accelerator);
}

void ReloadButton::ExecuteCommand(int command_id, int event_flags) {
  int browser_command = 0;
  switch (command_id) {
    case IDS_RELOAD_MENU_NORMAL_RELOAD_ITEM:
      browser_command = IDC_RELOAD;
      break;
    case IDS_RELOAD_MENU_HARD_RELOAD_ITEM:
      browser_command = IDC_RELOAD_BYPASSING_CACHE;
      break;
    case IDS_RELOAD_MENU_EMPTY_AND_HARD_RELOAD_ITEM:
      browser_command = IDC_RELOAD_CLEARING_CACHE;
      break;
    default:
      NOTREACHED();
  }
  ExecuteBrowserCommand(browser_command, event_flags);
}

std::unique_ptr<ui::SimpleMenuModel> ReloadButton::CreateMenuModel() {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  for (size_t i = 0; i < arraysize(kReloadMenuItems); ++i)
    menu_model->AddItemWithStringId(kReloadMenuItems[i], kReloadMenuItems[i]);
  return menu_model;
}

void ReloadButton::ExecuteBrowserCommand(int command, int event_flags) {
  if (!command_updater_)
    return;
  command_updater_->ExecuteCommandWithDisposition(
      command, ui::DispositionFromEventFlags(event_flags));
}

void ReloadButton::ChangeModeInternal(Mode mode) {
  const ui::ThemeProvider* tp = GetThemeProvider();
  // |tp| can be NULL in unit tests.
  if (tp) {
    const gfx::VectorIcon& icon = GetIconForMode(mode == Mode::kReload);
    const SkColor normal_color =
        tp->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
    const SkColor disabled_color =
        tp->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_INACTIVE);
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(icon, normal_color));
    SetImage(views::Button::STATE_DISABLED,
             gfx::CreateVectorIcon(icon, disabled_color));
    set_ink_drop_base_color(normal_color);
  }

  visible_mode_ = mode;
  SchedulePaint();
}

void ReloadButton::OnDoubleClickTimer() {
  if (!IsMenuShowing())
    ChangeMode(intended_mode_, false);
}

void ReloadButton::OnStopToReloadTimer() {
  DCHECK(!IsMenuShowing());
  ChangeMode(intended_mode_, true);
}
