// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_observer_hud.h"

#include "ash/shell_window_ids.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/gfx/transform.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

const int kPointRadius = 20;
const int kColors[] = {
  static_cast<int>(SK_ColorYELLOW),
  static_cast<int>(SK_ColorGREEN),
  static_cast<int>(SK_ColorRED),
  static_cast<int>(SK_ColorBLUE),
  static_cast<int>(SK_ColorGRAY),
  static_cast<int>(SK_ColorMAGENTA),
  static_cast<int>(SK_ColorCYAN),
  static_cast<int>(SK_ColorWHITE),
  static_cast<int>(SK_ColorBLACK)
};
const int kAlpha = 0x60;
const int kMaxPaths = arraysize(kColors);
const int kReducedScale = 10;

class TouchHudCanvas : public views::View {
 public:
  explicit TouchHudCanvas(TouchObserverHUD* owner)
      : owner_(owner),
        path_index_(0),
        color_index_(0),
        scale_(1) {
    gfx::Display display = Shell::GetScreen()->GetPrimaryDisplay();
    gfx::Rect bounds = display.bounds();
    SetPaintToLayer(true);
    SetFillsBoundsOpaquely(false);
  }

  virtual ~TouchHudCanvas() {}

  void SetScale(int scale) {
    if (scale_ == scale)
      return;
    scale_ = scale;
    gfx::Transform transform;
    transform.Scale(1. / scale_, 1. / scale_);
    layer()->SetTransform(transform);
  }

  int scale() const { return scale_; }

  void Start(int id, const gfx::Point& point) {
    paths_[path_index_].reset();
    colors_[path_index_] = SkColorSetA(kColors[color_index_], kAlpha);
    color_index_ = (color_index_ + 1) % arraysize(kColors);
    touch_id_to_path_[id] = path_index_;
    path_index_ = (path_index_ + 1) % kMaxPaths;
    AddPoint(id, point);
    SchedulePaint();
  }

  void AddPoint(int id, const gfx::Point& point) {
    SkPoint last;
    int path_id = touch_id_to_path_[id];
    SkScalar x = SkIntToScalar(point.x());
    SkScalar y = SkIntToScalar(point.y());
    if (!paths_[path_id].getLastPt(&last) || x != last.x() || y != last.y())
      paths_[path_id].addCircle(x, y, SkIntToScalar(kPointRadius));
    SchedulePaint();
  }

 private:
  // Overridden from views::View.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawColor(SK_ColorBLACK, SkXfermode::kClear_Mode);
    canvas->DrawColor(SkColorSetARGB(0, 0, 0, 0));

    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    for (size_t i = 0; i < arraysize(paths_); ++i) {
      if (paths_[i].countPoints() == 0)
        continue;
      paint.setColor(colors_[i]);
      canvas->DrawPath(paths_[i], paint);
    }
  }

  TouchObserverHUD* owner_;
  SkPath paths_[kMaxPaths];
  SkColor colors_[kMaxPaths];

  int path_index_;
  int color_index_;
  int scale_;

  std::map<int, int> touch_id_to_path_;

  DISALLOW_COPY_AND_ASSIGN(TouchHudCanvas);
};

TouchObserverHUD::TouchObserverHUD() {
  views::View* content = new views::View;

  canvas_ = new TouchHudCanvas(this);
  content->AddChildView(canvas_);

  gfx::Display display = Shell::GetScreen()->GetPrimaryDisplay();
  gfx::Rect display_bounds = display.bounds();
  canvas_->SetBoundsRect(display_bounds);
  content->SetBoundsRect(display_bounds);

  label_container_ = new views::View;
  label_container_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, 0));

  for (int i = 0; i < kMaxTouchPoints; ++i) {
    touch_status_[i] = ui::ET_UNKNOWN;
    touch_labels_[i] = new views::Label;
    touch_labels_[i]->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
    touch_labels_[i]->SetShadowColors(SK_ColorWHITE,
                                      SK_ColorWHITE);
    touch_labels_[i]->SetShadowOffset(1, 1);
    label_container_->AddChildView(touch_labels_[i]);
  }
  label_container_->SetX(0);
  label_container_->SetY(display_bounds.height() / kReducedScale);
  label_container_->SetSize(label_container_->GetPreferredSize());
  label_container_->SetVisible(false);
  content->AddChildView(label_container_);

  widget_ = new views::Widget();
  views::Widget::InitParams
      params(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.transparent = true;
  params.can_activate = false;
  params.accept_events = false;
  params.bounds = display_bounds;
  params.parent = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(),
      internal::kShellWindowId_OverlayContainer);
  widget_->Init(params);
  widget_->SetContentsView(content);
  widget_->StackAtTop();
  widget_->Show();

  // The TouchObserverHUD's lifetime is always more than |widget_|. The
  // |widget_| is unset from the OnWidgetClosing callback.
  widget_->AddObserver(this);
}

TouchObserverHUD::~TouchObserverHUD() {
  // The widget should have already been destroyed.
  DCHECK(!widget_);
}

void TouchObserverHUD::ChangeToNextMode() {
  if (widget_->IsVisible()) {
    if (canvas_->scale() == kReducedScale) {
      widget_->Hide();
    } else {
      label_container_->SetVisible(true);
      canvas_->SetScale(kReducedScale);
    }
  } else {
    canvas_->SetScale(1);
    label_container_->SetVisible(false);
    widget_->Show();
  }
}

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

void TouchObserverHUD::OnTouchEvent(ui::TouchEvent* event) {
  if (event->touch_id() >= kMaxTouchPoints)
    return;

  if (event->type() != ui::ET_TOUCH_CANCELLED)
    touch_positions_[event->touch_id()] = event->root_location();
  if (event->type() == ui::ET_TOUCH_PRESSED)
    canvas_->Start(event->touch_id(), touch_positions_[event->touch_id()]);
  else
    canvas_->AddPoint(event->touch_id(), touch_positions_[event->touch_id()]);
  touch_status_[event->touch_id()] = event->type();

  UpdateTouchPointLabel(event->touch_id());
  label_container_->SetSize(label_container_->GetPreferredSize());
}

void TouchObserverHUD::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(widget, widget_);
  widget_ = NULL;
}

}  // namespace internal
}  // namespace ash
