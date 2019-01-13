// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/back_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_sink.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/widget/widget.h"

namespace ash {

BackButton::BackButton(ShelfView* shelf_view) : ShelfControlButton(shelf_view) {
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ASH_SHELF_BACK_BUTTON_TITLE));
}

BackButton::~BackButton() = default;

void BackButton::OnGestureEvent(ui::GestureEvent* event) {
  ShelfButton::OnGestureEvent(event);
  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_TAP_DOWN) {
    GenerateAndSendBackEvent(event->type());
  }
}

bool BackButton::OnMousePressed(const ui::MouseEvent& event) {
  ShelfButton::OnMousePressed(event);
  GenerateAndSendBackEvent(event.type());
  return true;
}

void BackButton::OnMouseReleased(const ui::MouseEvent& event) {
  ShelfButton::OnMouseReleased(event);
  GenerateAndSendBackEvent(event.type());
}

void BackButton::PaintButtonContents(gfx::Canvas* canvas) {
  // Use PaintButtonContents instead of SetImage so the icon gets drawn at
  // |GetCenterPoint| coordinates instead of always in the center.
  gfx::ImageSkia img = CreateVectorIcon(kShelfBackIcon, SK_ColorWHITE);
  canvas->DrawImageInt(img, GetCenterPoint().x() - img.width() / 2,
                       GetCenterPoint().y() - img.height() / 2);
}

void BackButton::GenerateAndSendBackEvent(
    const ui::EventType& original_event_type) {
  ui::EventType new_event_type;
  switch (original_event_type) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_GESTURE_TAP_DOWN:
      new_event_type = ui::ET_KEY_PRESSED;
      break;
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_GESTURE_TAP:
      new_event_type = ui::ET_KEY_RELEASED;
      base::RecordAction(base::UserMetricsAction("Tablet_BackButton"));
      break;
    default:
      return;
  }

  // Send the back event to the root window of the back button's widget.
  const views::Widget* widget = GetWidget();
  if (widget && widget->GetNativeWindow()) {
    aura::Window* root_window = widget->GetNativeWindow()->GetRootWindow();
    ui::KeyEvent key_event(new_event_type, ui::VKEY_BROWSER_BACK, ui::EF_NONE);
    ignore_result(
        root_window->GetHost()->event_sink()->OnEventFromSource(&key_event));
  }
}

}  // namespace ash
