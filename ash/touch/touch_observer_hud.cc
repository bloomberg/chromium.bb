// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_observer_hud.h"

#include "ash/shell_window_ids.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/event.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

TouchObserverHUD::TouchObserverHUD() {
  views::View* content = new views::View;
  content->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, 0));

  for (int i = 0; i < kMaxTouchPoints; ++i) {
    touch_status_[i] = ui::ET_UNKNOWN;
    touch_labels_[i] = new views::Label;
    touch_labels_[i]->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
    touch_labels_[i]->SetShadowColors(SK_ColorWHITE,
                                      SK_ColorWHITE);
    touch_labels_[i]->SetShadowOffset(1, 1);
    touch_labels_[i]->SetVisible(false);
    content->AddChildView(touch_labels_[i]);
  }

  widget_.reset(new views::Widget());
  views::Widget::InitParams
      params(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.transparent = true;
  params.can_activate = false;
  params.bounds = gfx::Rect(content->GetPreferredSize());
  params.parent = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_OverlayContainer);
  widget_->Init(params);
  widget_->SetContentsView(content);
  widget_->StackAtTop();
  widget_->Show();
}

TouchObserverHUD::~TouchObserverHUD() {}

void TouchObserverHUD::UpdateTouchPointLabel(int index) {
  const char* status = NULL;
  switch (touch_status_[index]) {
    case ui::ET_UNKNOWN:
      status = " ";
      break;
    case ui::ET_TOUCH_PRESSED:
      status = "P";
      break;
    case ui::ET_TOUCH_MOVED:
      status = "M";
      break;
    case ui::ET_TOUCH_RELEASED:
      status = "R";
      break;
    case ui::ET_TOUCH_CANCELLED:
      status = "C";
      break;
    default:
      status = "?";
      break;
  }
  std::string string = base::StringPrintf("%2d: %s %s",
      index, status, touch_positions_[index].ToString().c_str());
  touch_labels_[index]->SetText(UTF8ToUTF16(string));
}

bool TouchObserverHUD::PreHandleKeyEvent(aura::Window* target,
                                         aura::KeyEvent* event) {
  return false;
}

bool TouchObserverHUD::PreHandleMouseEvent(aura::Window* target,
                                           aura::MouseEvent* event) {
  return false;
}

ui::TouchStatus TouchObserverHUD::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  if (event->touch_id() >= kMaxTouchPoints)
    return ui::TOUCH_STATUS_UNKNOWN;

  if (event->type() != ui::ET_TOUCH_CANCELLED)
    touch_positions_[event->touch_id()] = event->root_location();
  touch_status_[event->touch_id()] = event->type();
  touch_labels_[event->touch_id()]->SetVisible(true);
  UpdateTouchPointLabel(event->touch_id());

  widget_->SetSize(widget_->GetContentsView()->GetPreferredSize());

  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus TouchObserverHUD::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;
}

}  // namespace internal
}  // namespace ash
