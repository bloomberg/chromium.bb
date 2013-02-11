// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_observer_hud.h"

#include "ash/shell_window_ids.h"
#include "base/json/json_string_value_serializer.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
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

#if defined(USE_X11)
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>

#include "ui/base/x/valuators.h"
#endif

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

const char* GetTouchEventLabel(ui::EventType type) {
  switch (type) {
    case ui::ET_UNKNOWN:
      return " ";
    case ui::ET_TOUCH_PRESSED:
      return "P";
    case ui::ET_TOUCH_MOVED:
      return "M";
    case ui::ET_TOUCH_RELEASED:
      return "R";
    case ui::ET_TOUCH_CANCELLED:
      return "C";
    default:
      break;
  }
  return "?";
}

int GetTrackingId(const ui::TouchEvent& event) {
  if (!event.HasNativeEvent())
    return 0;
#if defined(USE_XI2_MT)
  ui::ValuatorTracker* valuators = ui::ValuatorTracker::GetInstance();
  float tracking_id;
  if (valuators->ExtractValuator(*event.native_event(),
                                 ui::ValuatorTracker::VAL_TRACKING_ID,
                                 &tracking_id)) {
    return static_cast<int>(tracking_id);
  }
#endif
  return 0;
}

int GetSourceDeviceId(const ui::TouchEvent& event) {
  if (!event.HasNativeEvent())
    return 0;
#if defined(USE_X11)
  XEvent* xev = event.native_event();
  return static_cast<XIDeviceEvent*>(xev->xcookie.data)->sourceid;
#endif
  return 0;
}

// A TouchPointLog represents a single touch-event of a touch point.
struct TouchPointLog {
 public:
  explicit TouchPointLog(const ui::TouchEvent& touch)
      : id(touch.touch_id()),
        type(touch.type()),
        location(touch.root_location()),
        timestamp(touch.time_stamp().InMillisecondsF()),
        radius_x(touch.radius_x()),
        radius_y(touch.radius_y()),
        pressure(touch.force()),
        tracking_id(GetTrackingId(touch)),
        source_device(GetSourceDeviceId(touch)) {
  }

  // Populates a dictionary value with all the information about the touch
  // point.
  scoped_ptr<DictionaryValue> GetAsDictionary() const {
    scoped_ptr<DictionaryValue> value(new DictionaryValue());

    value->SetInteger("id", id);
    value->SetString("type", std::string(GetTouchEventLabel(type)));
    value->SetString("location", location.ToString());
    value->SetDouble("timestamp", timestamp);
    value->SetDouble("radius_x", radius_x);
    value->SetDouble("radius_y", radius_y);
    value->SetDouble("pressure", pressure);
    value->SetInteger("tracking_id", tracking_id);
    value->SetInteger("source_device", source_device);

    return value.Pass();
  }

  int id;
  ui::EventType type;
  gfx::Point location;
  double timestamp;
  float radius_x;
  float radius_y;
  float pressure;
  int tracking_id;
  int source_device;
};

// A TouchTrace keeps track of all the touch events of a single touch point
// (starting from a touch-press and ending at touch-release).
class TouchTrace {
 public:
  TouchTrace() {
  }

  void AddTouchPoint(const ui::TouchEvent& touch) {
    log_.push_back(TouchPointLog(touch));
  }

  bool empty() const { return log_.empty(); }

  // Returns a list containing data from all events for the touch point.
  scoped_ptr<ListValue> GetAsList() const {
    scoped_ptr<ListValue> list(new ListValue());
    for (std::vector<TouchPointLog>::const_iterator i = log_.begin();
        i != log_.end(); ++i) {
      list->Append((*i).GetAsDictionary().release());
    }

    return list.Pass();
  }

  void Reset() {
    log_.clear();
  }

 private:
  std::vector<TouchPointLog> log_;

  DISALLOW_COPY_AND_ASSIGN(TouchTrace);
};

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

  void Start(const ui::TouchEvent& touch) {
    int id = touch.touch_id();
    paths_[path_index_].reset();
    traces_[path_index_].Reset();
    colors_[path_index_] = SkColorSetA(kColors[color_index_], kAlpha);
    color_index_ = (color_index_ + 1) % arraysize(kColors);
    touch_id_to_path_[id] = path_index_;
    path_index_ = (path_index_ + 1) % kMaxPaths;
    AddPoint(touch);
    SchedulePaint();
  }

  void AddPoint(const ui::TouchEvent& touch) {
    int id = touch.touch_id();
    const gfx::Point& point = touch.root_location();
    int path_id = touch_id_to_path_[id];
    SkScalar x = SkIntToScalar(point.x());
    SkScalar y = SkIntToScalar(point.y());
    SkPoint last;
    if (!paths_[path_id].getLastPt(&last) || x != last.x() || y != last.y()) {
      paths_[path_id].addCircle(x, y, SkIntToScalar(kPointRadius));
      traces_[path_id].AddTouchPoint(touch);
    }
    SchedulePaint();
  }

  void Clear() {
    path_index_ = 0;
    color_index_ = 0;
    for (size_t i = 0; i < arraysize(paths_); ++i) {
      paths_[i].reset();
      traces_[i].Reset();
    }

    SchedulePaint();
  }

  scoped_ptr<ListValue> GetAsList() const {
    scoped_ptr<ListValue> list(new ListValue());
    for (size_t i = 0; i < arraysize(traces_); ++i) {
      if (!traces_[i].empty())
        list->Append(traces_[i].GetAsList().release());
    }
    return list.Pass();
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
  TouchTrace traces_[kMaxPaths];

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
  // |widget_| is unset from the OnWidgetDestroying callback.
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

void TouchObserverHUD::Clear() {
  if (widget_->IsVisible())
    canvas_->Clear();
}

std::string TouchObserverHUD::GetLogAsString() const {
  std::string string;
  scoped_ptr<ListValue> list = canvas_->GetAsList();
  JSONStringValueSerializer json(&string);
  json.set_pretty_print(true);
  return json.Serialize(*list) ? string : std::string();
}

void TouchObserverHUD::UpdateTouchPointLabel(int index) {
  std::string string = base::StringPrintf("%2d: %s %s (%.4f)",
      index, GetTouchEventLabel(touch_status_[index]),
      touch_positions_[index].ToString().c_str(),
      touch_radius_[index]);
  touch_labels_[index]->SetText(UTF8ToUTF16(string));
}

void TouchObserverHUD::OnTouchEvent(ui::TouchEvent* event) {
  if (event->touch_id() >= kMaxTouchPoints)
    return;

  if (event->type() != ui::ET_TOUCH_CANCELLED)
    touch_positions_[event->touch_id()] = event->root_location();

  touch_radius_[event->touch_id()] = std::max(event->radius_x(),
                                              event->radius_y());

  if (event->type() == ui::ET_TOUCH_PRESSED)
    canvas_->Start(*event);
  else if (event->type() != ui::ET_TOUCH_CANCELLED)
    canvas_->AddPoint(*event);
  touch_status_[event->touch_id()] = event->type();

  UpdateTouchPointLabel(event->touch_id());
  label_container_->SetSize(label_container_->GetPreferredSize());
}

void TouchObserverHUD::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, widget_);
  widget_ = NULL;
}

}  // namespace internal
}  // namespace ash
