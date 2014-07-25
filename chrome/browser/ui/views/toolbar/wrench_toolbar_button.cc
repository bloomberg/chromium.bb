// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/wrench_toolbar_button.h"

#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "extensions/common/feature_switch.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/views/metrics.h"
#include "ui/views/painter.h"

// static
bool WrenchToolbarButton::g_open_wrench_immediately_for_testing = false;

WrenchToolbarButton::WrenchToolbarButton(ToolbarView* toolbar_view)
    : views::MenuButton(NULL, base::string16(), toolbar_view, false),
      wrench_icon_painter_(new WrenchIconPainter(this)),
      toolbar_view_(toolbar_view),
      allow_extension_dragging_(
          extensions::FeatureSwitch::extension_action_redesign()->IsEnabled()),
      weak_factory_(this) {
}

WrenchToolbarButton::~WrenchToolbarButton() {
}

void WrenchToolbarButton::SetSeverity(WrenchIconPainter::Severity severity,
                                      bool animate) {
  wrench_icon_painter_->SetSeverity(severity, animate);
  SchedulePaint();
}

gfx::Size WrenchToolbarButton::GetPreferredSize() const {
  return ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_TOOLBAR_BEZEL_HOVER)->size();
}

void WrenchToolbarButton::OnPaint(gfx::Canvas* canvas) {
  views::MenuButton::OnPaint(canvas);
  wrench_icon_painter_->Paint(canvas,
                              GetThemeProvider(),
                              gfx::Rect(size()),
                              WrenchIconPainter::BEZEL_NONE);
}

void WrenchToolbarButton::ScheduleWrenchIconPaint() {
  SchedulePaint();
}

bool WrenchToolbarButton::GetDropFormats(
    int* formats, std::set<ui::OSExchangeData::CustomFormat>* custom_formats) {
  return allow_extension_dragging_ ?
      BrowserActionDragData::GetDropFormats(custom_formats) :
      views::View::GetDropFormats(formats, custom_formats);
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
  if (!g_open_wrench_immediately_for_testing) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&WrenchToolbarButton::ShowOverflowMenu,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(views::GetMenuShowDelay()));
  } else {
    ShowOverflowMenu();
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

void WrenchToolbarButton::ShowOverflowMenu() {
  toolbar_view_->ShowAppMenu(true);  // For drop.
}
