// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/wrench_toolbar_button.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/layout_constants.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/wrench_menu.h"
#include "extensions/common/feature_switch.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/menu/menu_listener.h"
#include "ui/views/metrics.h"
#include "ui/views/painter.h"

// static
bool WrenchToolbarButton::g_open_app_immediately_for_testing = false;

WrenchToolbarButton::WrenchToolbarButton(ToolbarView* toolbar_view)
    : views::MenuButton(NULL, base::string16(), toolbar_view, false),
      wrench_icon_painter_(nullptr),
      toolbar_view_(toolbar_view),
      allow_extension_dragging_(
          extensions::FeatureSwitch::extension_action_redesign()
              ->IsEnabled()),
      weak_factory_(this) {
  if (!ui::MaterialDesignController::IsModeMaterial())
    wrench_icon_painter_.reset(new WrenchIconPainter(this));
}

WrenchToolbarButton::~WrenchToolbarButton() {
}

void WrenchToolbarButton::SetSeverity(WrenchIconPainter::Severity severity,
                                      bool animate) {
  if (ui::MaterialDesignController::IsModeMaterial())
    return;

  wrench_icon_painter_->SetSeverity(severity, animate);
  SchedulePaint();
}

void WrenchToolbarButton::ShowMenu(bool for_drop) {
  if (menu_ && menu_->IsShowing())
    return;

#if defined(USE_AURA)
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller && keyboard_controller->keyboard_visible()) {
    keyboard_controller->HideKeyboard(
        keyboard::KeyboardController::HIDE_REASON_AUTOMATIC);
  }
#endif

  Browser* browser = toolbar_view_->browser();

  menu_.reset(new WrenchMenu(browser, for_drop ? WrenchMenu::FOR_DROP : 0));
  menu_model_.reset(new WrenchMenuModel(toolbar_view_, browser));
  menu_->Init(menu_model_.get());

  FOR_EACH_OBSERVER(views::MenuListener, menu_listeners_, OnMenuOpened());

  menu_->RunMenu(this);
}

void WrenchToolbarButton::CloseMenu() {
  if (menu_)
    menu_->CloseMenu();
  menu_.reset();
}

bool WrenchToolbarButton::IsMenuShowing() const {
  return menu_ && menu_->IsShowing();
}

void WrenchToolbarButton::AddMenuListener(views::MenuListener* listener) {
  menu_listeners_.AddObserver(listener);
}

void WrenchToolbarButton::RemoveMenuListener(views::MenuListener* listener) {
  menu_listeners_.RemoveObserver(listener);
}

gfx::Size WrenchToolbarButton::GetPreferredSize() const {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    gfx::Size size(image()->GetPreferredSize());
    ui::ThemeProvider* provider = GetThemeProvider();
    if (provider) {
      gfx::Insets insets(GetLayoutInsets(TOOLBAR_BUTTON));
      size.Enlarge(insets.width(), insets.height());
    }
    return size;
  }

  return ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_TOOLBAR_BEZEL_HOVER)->size();
}

void WrenchToolbarButton::ScheduleWrenchIconPaint() {
  SchedulePaint();
}

const char* WrenchToolbarButton::GetClassName() const {
  return "WrenchToolbarButton";
}

bool WrenchToolbarButton::GetDropFormats(
    int* formats,
    std::set<ui::Clipboard::FormatType>* format_types) {
  return allow_extension_dragging_ ?
      BrowserActionDragData::GetDropFormats(format_types) :
      views::View::GetDropFormats(formats, format_types);
}
bool WrenchToolbarButton::AreDropTypesRequired() {
  return allow_extension_dragging_ ?
      BrowserActionDragData::AreDropTypesRequired() :
      views::View::AreDropTypesRequired();
}
bool WrenchToolbarButton::CanDrop(const ui::OSExchangeData& data) {
  return allow_extension_dragging_ ?
      BrowserActionDragData::CanDrop(data,
                                     toolbar_view_->browser()->profile()) :
      views::View::CanDrop(data);
}

void WrenchToolbarButton::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(allow_extension_dragging_);
  DCHECK(!weak_factory_.HasWeakPtrs());
  if (!g_open_app_immediately_for_testing) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&WrenchToolbarButton::ShowMenu,
                              weak_factory_.GetWeakPtr(), true),
        base::TimeDelta::FromMilliseconds(views::GetMenuShowDelay()));
  } else {
    ShowMenu(true);
  }
}

int WrenchToolbarButton::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(allow_extension_dragging_);
  return ui::DragDropTypes::DRAG_MOVE;
}

void WrenchToolbarButton::OnDragExited() {
  DCHECK(allow_extension_dragging_);
  weak_factory_.InvalidateWeakPtrs();
}

int WrenchToolbarButton::OnPerformDrop(const ui::DropTargetEvent& event) {
  DCHECK(allow_extension_dragging_);
  return ui::DragDropTypes::DRAG_MOVE;
}

void WrenchToolbarButton::OnPaint(gfx::Canvas* canvas) {
  views::MenuButton::OnPaint(canvas);
  if (ui::MaterialDesignController::IsModeMaterial())
    return;
  wrench_icon_painter_->Paint(canvas,
                              GetThemeProvider(),
                              gfx::Rect(size()),
                              WrenchIconPainter::BEZEL_NONE);
}
