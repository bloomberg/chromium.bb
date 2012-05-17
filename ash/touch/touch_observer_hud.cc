// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_observer_hud.h"

#include "ash/shell_window_ids.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/aura/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/monitor.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

const int kMaxPaths = 15;
const int kScale = 10;
const int kColors[] = {
  SK_ColorYELLOW,
  SK_ColorGREEN,
  SK_ColorRED,
  SK_ColorBLUE,
  SK_ColorMAGENTA,
  SK_ColorCYAN,
  SK_ColorWHITE,
  SK_ColorBLACK
};

class TouchHudCanvas : public views::View {
 public:
  explicit TouchHudCanvas(TouchObserverHUD* owner)
      : owner_(owner),
        path_index_(0),
        color_index_(0) {
    gfx::Monitor monitor = gfx::Screen::GetPrimaryMonitor();
    gfx::Rect bounds = monitor.bounds();
    size_.set_width(bounds.width() / kScale);
    size_.set_height(bounds.height() / kScale);
  }

  virtual ~TouchHudCanvas() {}

  void Start(int id, const gfx::Point& point) {
    paths_[path_index_].reset();
    paths_[path_index_].moveTo(SkIntToScalar(point.x() / kScale),
                      SkIntToScalar(point.y() / kScale));
    colors_[path_index_] = kColors[color_index_];
    color_index_ = (color_index_ + 1) % arraysize(kColors);

    touch_id_to_path_[id] = path_index_;
    path_index_ = (path_index_ + 1) % kMaxPaths;
    SchedulePaint();
  }

  void Update(int id, gfx::Point& to) {
    SkPoint last;
    int path_id = touch_id_to_path_[id];
    SkScalar x = SkIntToScalar(to.x() / kScale);
    SkScalar y = SkIntToScalar(to.y() / kScale);
    if (!paths_[path_id].getLastPt(&last) || x != last.x() || y != last.y())
      paths_[path_id].lineTo(x, y);
    SchedulePaint();
  }

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return size_;
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawColor(SK_ColorBLACK, SkXfermode::kClear_Mode);
    canvas->DrawColor(SkColorSetARGB(25, 0, 0, 0));

    SkPaint paint;
    paint.setStrokeWidth(SkIntToScalar(2));
    paint.setStyle(SkPaint::kStroke_Style);
    for (size_t i = 0; i < arraysize(paths_); ++i) {
      if (paths_[i].countPoints() == 0)
        continue;
      paint.setColor(colors_[i]);
      if (paths_[i].countPoints() == 1) {
        SkPoint point = paths_[i].getPoint(0);
        canvas->sk_canvas()->drawPoint(point.x(), point.y(), paint);
      } else {
        canvas->DrawPath(paths_[i], paint);
      }
    }
  }

  TouchObserverHUD* owner_;
  SkPath paths_[kMaxPaths];
  SkColor colors_[kMaxPaths];

  int path_index_;
  int color_index_;

  std::map<int, int> touch_id_to_path_;

  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(TouchHudCanvas);
};

TouchObserverHUD::TouchObserverHUD() {
  views::View* content = new views::View;
  content->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, 0));

  canvas_ = new TouchHudCanvas(this);
  content->AddChildView(canvas_);
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
  params.accept_events = false;
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
  if (event->type() == ui::ET_TOUCH_PRESSED)
    canvas_->Start(event->touch_id(), touch_positions_[event->touch_id()]);
  else
    canvas_->Update(event->touch_id(), touch_positions_[event->touch_id()]);
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
