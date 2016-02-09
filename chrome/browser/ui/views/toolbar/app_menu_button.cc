// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/app_menu_button.h"

#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "extensions/common/feature_switch.h"
#include "grit/theme_resources.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/animation/button_ink_drop_delegate.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/menu/menu_listener.h"
#include "ui/views/metrics.h"
#include "ui/views/painter.h"

// static
bool AppMenuButton::g_open_app_immediately_for_testing = false;

AppMenuButton::AppMenuButton(ToolbarView* toolbar_view)
    : views::MenuButton(base::string16(), toolbar_view, false),
      severity_(AppMenuIconPainter::SEVERITY_NONE),
      toolbar_view_(toolbar_view),
      allow_extension_dragging_(
          extensions::FeatureSwitch::extension_action_redesign()->IsEnabled()),
      destroyed_(nullptr),
      margin_trailing_(0),
      ink_drop_delegate_(new views::ButtonInkDropDelegate(this, this)),
      weak_factory_(this) {
  set_ink_drop_delegate(ink_drop_delegate_.get());
  if (!ui::MaterialDesignController::IsModeMaterial())
    icon_painter_.reset(new AppMenuIconPainter(this));

  const int kInkDropLargeSize = 32;
  const int kInkDropLargeCornerRadius = 5;
  const int kInkDropSmallSize = 24;
  const int kInkDropSmallCornerRadius = 2;
  ink_drop_delegate()->SetInkDropSize(
      kInkDropLargeSize, kInkDropLargeCornerRadius, kInkDropSmallSize,
      kInkDropSmallCornerRadius);
}

AppMenuButton::~AppMenuButton() {
  if (destroyed_)
    *destroyed_ = true;
}

void AppMenuButton::SetSeverity(AppMenuIconPainter::Severity severity,
                                bool animate) {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    severity_ = severity;
    UpdateIcon();
    return;
  }

  icon_painter_->SetSeverity(severity, animate);
  SchedulePaint();
}

void AppMenuButton::ShowMenu(bool for_drop) {
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

  menu_.reset(new AppMenu(browser, for_drop ? AppMenu::FOR_DROP : 0));
  menu_model_.reset(new AppMenuModel(toolbar_view_, browser));
  menu_->Init(menu_model_.get());

  FOR_EACH_OBSERVER(views::MenuListener, menu_listeners_, OnMenuOpened());

  // Because running the menu below spins a nested message loop, |this| can be
  // deleted by the time RunMenu() returns. To detect this, we set |destroyed_|
  // (which is normally null) to point to a local. If our destructor runs during
  // RunMenu(), then this local will be set to true on return, and we'll know
  // it's not safe to access any member variables.
  bool destroyed = false;
  destroyed_ = &destroyed;

  ink_drop_delegate()->OnAction(views::InkDropState::ACTIVATED);

  base::TimeTicks menu_open_time = base::TimeTicks::Now();
  menu_->RunMenu(this);

  if (!for_drop) {
    // Record the time-to-action for the menu. We don't record in the case of a
    // drag-and-drop command because menus opened for drag-and-drop don't block
    // the message loop.
    UMA_HISTOGRAM_TIMES("Toolbar.AppMenuTimeToAction",
                        base::TimeTicks::Now() - menu_open_time);
  }

  if (!destroyed) {
    ink_drop_delegate()->OnAction(views::InkDropState::DEACTIVATED);
    destroyed_ = nullptr;
  }
}

void AppMenuButton::CloseMenu() {
  if (menu_)
    menu_->CloseMenu();
  menu_.reset();
}

bool AppMenuButton::IsMenuShowing() const {
  return menu_ && menu_->IsShowing();
}

void AppMenuButton::AddMenuListener(views::MenuListener* listener) {
  menu_listeners_.AddObserver(listener);
}

void AppMenuButton::RemoveMenuListener(views::MenuListener* listener) {
  menu_listeners_.RemoveObserver(listener);
}

gfx::Size AppMenuButton::GetPreferredSize() const {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    gfx::Size size(image()->GetPreferredSize());
    const ui::ThemeProvider* provider = GetThemeProvider();
    if (provider) {
      gfx::Insets insets(GetLayoutInsets(TOOLBAR_BUTTON));
      size.Enlarge(insets.width(), insets.height());
    }
    return size;
  }

  return ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_TOOLBAR_BEZEL_HOVER)->size();
}

void AppMenuButton::ScheduleAppMenuIconPaint() {
  SchedulePaint();
}

void AppMenuButton::UpdateIcon() {
  DCHECK(ui::MaterialDesignController::IsModeMaterial());
  SkColor color = gfx::kPlaceholderColor;
  switch (severity_) {
    case AppMenuIconPainter::SEVERITY_NONE:
      color = GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
      break;
    case AppMenuIconPainter::SEVERITY_LOW:
      color = gfx::kGoogleGreen700;
      break;
    case AppMenuIconPainter::SEVERITY_MEDIUM:
      color = gfx::kGoogleYellow700;
      break;
    case AppMenuIconPainter::SEVERITY_HIGH:
      color = gfx::kGoogleRed700;
      break;
  }

  // TODO(estade): find a home for this constant.
  const int kButtonSize = 16;
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(gfx::VectorIconId::BROWSER_TOOLS, kButtonSize,
                                 color));
}

void AppMenuButton::SetTrailingMargin(int margin) {
  margin_trailing_ = margin;

  UpdateThemedBorder();

  const int inset = LabelButton::kFocusRectInset;
  SetFocusPainter(views::Painter::CreateDashedFocusPainterWithInsets(
      gfx::Insets(inset, inset, inset, inset + margin)));
  InvalidateLayout();
}

void AppMenuButton::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  image()->SetPaintToLayer(true);
  image()->SetFillsBoundsOpaquely(false);
  views::MenuButton::AddInkDropLayer(ink_drop_layer);
}

void AppMenuButton::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  views::MenuButton::RemoveInkDropLayer(ink_drop_layer);
  image()->SetFillsBoundsOpaquely(true);
  image()->SetPaintToLayer(false);
}

gfx::Point AppMenuButton::CalculateInkDropCenter() const {
  // ToolbarView extends the bounds of the app button to the right in maximized
  // mode. So instead of using the center point of local bounds, we use the
  // center point (adjusted for RTL layouts) of the preferred size, which
  // doesn't change in maximized mode.
  const int visible_width = GetPreferredSize().width();
  return gfx::Point(
      (GetMirroredXWithWidthInView(0, visible_width) + visible_width) / 2,
      height() / 2);
}

const char* AppMenuButton::GetClassName() const {
  return "AppMenuButton";
}

scoped_ptr<views::LabelButtonBorder> AppMenuButton::CreateDefaultBorder()
    const {
  scoped_ptr<views::LabelButtonBorder> border =
      MenuButton::CreateDefaultBorder();

  // Adjust border insets to follow the margin change,
  // which will be reflected in where the border is painted
  // through GetThemePaintRect().
  gfx::Insets insets(border->GetInsets());
  insets += gfx::Insets(0, 0, 0, margin_trailing_);
  border->set_insets(insets);

  return border;
}

gfx::Rect AppMenuButton::GetThemePaintRect() const {
  gfx::Rect rect(MenuButton::GetThemePaintRect());
  rect.Inset(0, 0, margin_trailing_, 0);
  return rect;
}

bool AppMenuButton::GetDropFormats(
    int* formats,
    std::set<ui::Clipboard::FormatType>* format_types) {
  return allow_extension_dragging_ ?
      BrowserActionDragData::GetDropFormats(format_types) :
      views::View::GetDropFormats(formats, format_types);
}

bool AppMenuButton::AreDropTypesRequired() {
  return allow_extension_dragging_ ?
      BrowserActionDragData::AreDropTypesRequired() :
      views::View::AreDropTypesRequired();
}

bool AppMenuButton::CanDrop(const ui::OSExchangeData& data) {
  return allow_extension_dragging_ ?
      BrowserActionDragData::CanDrop(data,
                                     toolbar_view_->browser()->profile()) :
      views::View::CanDrop(data);
}

void AppMenuButton::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(allow_extension_dragging_);
  DCHECK(!weak_factory_.HasWeakPtrs());
  if (!g_open_app_immediately_for_testing) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&AppMenuButton::ShowMenu, weak_factory_.GetWeakPtr(), true),
        base::TimeDelta::FromMilliseconds(views::GetMenuShowDelay()));
  } else {
    ShowMenu(true);
  }
}

int AppMenuButton::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(allow_extension_dragging_);
  return ui::DragDropTypes::DRAG_MOVE;
}

void AppMenuButton::OnDragExited() {
  DCHECK(allow_extension_dragging_);
  weak_factory_.InvalidateWeakPtrs();
}

int AppMenuButton::OnPerformDrop(const ui::DropTargetEvent& event) {
  DCHECK(allow_extension_dragging_);
  return ui::DragDropTypes::DRAG_MOVE;
}

void AppMenuButton::OnPaint(gfx::Canvas* canvas) {
  views::MenuButton::OnPaint(canvas);
  if (ui::MaterialDesignController::IsModeMaterial())
    return;
  // Use GetPreferredSize() to center the icon inside the visible bounds rather
  // than the whole size() (which may refer to hit test region extended to the
  // end of the toolbar in maximized mode).
  icon_painter_->Paint(canvas, GetThemeProvider(),
                       gfx::Rect(GetPreferredSize()),
                       AppMenuIconPainter::BEZEL_NONE);
}
