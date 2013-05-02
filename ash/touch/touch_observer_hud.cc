// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_observer_hud.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/property_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/linear_animation.h"
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
const SkColor kColors[] = {
  SK_ColorYELLOW,
  SK_ColorGREEN,
  SK_ColorRED,
  SK_ColorBLUE,
  SK_ColorGRAY,
  SK_ColorMAGENTA,
  SK_ColorCYAN,
  SK_ColorWHITE,
  SK_ColorBLACK,
  SkColorSetRGB(0xFF, 0x8C, 0x00),
  SkColorSetRGB(0x8B, 0x45, 0x13),
  SkColorSetRGB(0xFF, 0xDE, 0xAD),
};
const int kAlpha = 0x60;
const SkColor kProjectionFillColor = SkColorSetRGB(0xF5, 0xF5, 0xDC);
const SkColor kProjectionStrokeColor = SK_ColorGRAY;
const int kProjectionAlpha = 0xB0;
const int kMaxPaths = arraysize(kColors);
const int kReducedScale = 10;
const int kFadeoutDurationInMs = 250;
const int kFadeoutFrameRate = 60;

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
  typedef std::vector<TouchPointLog>::iterator iterator;
  typedef std::vector<TouchPointLog>::const_iterator const_iterator;
  typedef std::vector<TouchPointLog>::reverse_iterator reverse_iterator;
  typedef std::vector<TouchPointLog>::const_reverse_iterator
      const_reverse_iterator;

  TouchTrace() {
  }

  void AddTouchPoint(const ui::TouchEvent& touch) {
    log_.push_back(TouchPointLog(touch));
  }

  const std::vector<TouchPointLog>& log() const { return log_; }

  bool active() const {
    return !log_.empty() && log_.back().type != ui::ET_TOUCH_RELEASED &&
        log_.back().type != ui::ET_TOUCH_CANCELLED;
  }

  // Returns a list containing data from all events for the touch point.
  scoped_ptr<ListValue> GetAsList() const {
    scoped_ptr<ListValue> list(new ListValue());
    for (const_iterator i = log_.begin(); i != log_.end(); ++i)
      list->Append((*i).GetAsDictionary().release());
    return list.Pass();
  }

  void Reset() {
    log_.clear();
  }

 private:
  std::vector<TouchPointLog> log_;

  DISALLOW_COPY_AND_ASSIGN(TouchTrace);
};

// A TouchLog keeps track of all touch events of all touch points.
class TouchLog {
 public:
  TouchLog() : next_trace_index_(0) {
  }

  void AddTouchPoint(const ui::TouchEvent& touch) {
    if (touch.type() == ui::ET_TOUCH_PRESSED)
      StartTrace(touch);
    AddToTrace(touch);
  }

  void Reset() {
    next_trace_index_ = 0;
    for (int i = 0; i < kMaxPaths; ++i)
      traces_[i].Reset();
  }

  scoped_ptr<ListValue> GetAsList() const {
    scoped_ptr<ListValue> list(new ListValue());
    for (int i = 0; i < kMaxPaths; ++i) {
      if (!traces_[i].log().empty())
        list->Append(traces_[i].GetAsList().release());
    }
    return list.Pass();
  }

  int GetTraceIndex(int touch_id) const {
    return touch_id_to_trace_index_.at(touch_id);
  }

  const TouchTrace* traces() const {
    return traces_;
  }

 private:
  void StartTrace(const ui::TouchEvent& touch) {
    // Find the first inactive spot; otherwise, overwrite the one
    // |next_trace_index_| is pointing to.
    int old_trace_index = next_trace_index_;
    do {
      if (!traces_[next_trace_index_].active())
        break;
      next_trace_index_ = (next_trace_index_ + 1) % kMaxPaths;
    } while (next_trace_index_ != old_trace_index);
    int touch_id = touch.touch_id();
    traces_[next_trace_index_].Reset();
    touch_id_to_trace_index_[touch_id] = next_trace_index_;
    next_trace_index_ = (next_trace_index_ + 1) % kMaxPaths;
  }

  void AddToTrace(const ui::TouchEvent& touch) {
    int touch_id = touch.touch_id();
    int trace_index = touch_id_to_trace_index_[touch_id];
    traces_[trace_index].AddTouchPoint(touch);
  }

  TouchTrace traces_[kMaxPaths];
  int next_trace_index_;

  std::map<int, int> touch_id_to_trace_index_;

  DISALLOW_COPY_AND_ASSIGN(TouchLog);
};

// TouchHudCanvas draws touch traces in |FULLSCREEN| and |REDUCED_SCALE| modes.
class TouchHudCanvas : public views::View {
 public:
  explicit TouchHudCanvas(const TouchLog& touch_log)
      : touch_log_(touch_log),
        scale_(1) {
    SetPaintToLayer(true);
    SetFillsBoundsOpaquely(false);

    paint_.setStyle(SkPaint::kFill_Style);
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

  void TouchPointAdded(int touch_id) {
    int trace_index = touch_log_.GetTraceIndex(touch_id);
    const TouchTrace& trace = touch_log_.traces()[trace_index];
    const TouchPointLog& point = trace.log().back();
    if (point.type == ui::ET_TOUCH_PRESSED)
      StartedTrace(trace_index);
    if (point.type != ui::ET_TOUCH_CANCELLED)
      AddedPointToTrace(trace_index);
  }

  void Clear() {
    for (int i = 0; i < kMaxPaths; ++i)
      paths_[i].reset();

    SchedulePaint();
  }

 private:
  void StartedTrace(int trace_index) {
    paths_[trace_index].reset();
    colors_[trace_index] = SkColorSetA(kColors[trace_index], kAlpha);
  }

  void AddedPointToTrace(int trace_index) {
    const TouchTrace& trace = touch_log_.traces()[trace_index];
    const TouchPointLog& point = trace.log().back();
    const gfx::Point& location = point.location;
    SkScalar x = SkIntToScalar(location.x());
    SkScalar y = SkIntToScalar(location.y());
    SkPoint last;
    if (!paths_[trace_index].getLastPt(&last) || x != last.x() ||
        y != last.y()) {
      paths_[trace_index].addCircle(x, y, SkIntToScalar(kPointRadius));
      SchedulePaint();
    }
  }

  // Overridden from views::View.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    for (int i = 0; i < kMaxPaths; ++i) {
      if (paths_[i].countPoints() == 0)
        continue;
      paint_.setColor(colors_[i]);
      canvas->DrawPath(paths_[i], paint_);
    }
  }

  SkPaint paint_;

  const TouchLog& touch_log_;
  SkPath paths_[kMaxPaths];
  SkColor colors_[kMaxPaths];

  int scale_;

  DISALLOW_COPY_AND_ASSIGN(TouchHudCanvas);
};

// TouchPointView draws a single touch point in |PROJECTION| mode. This object
// manages its own lifetime and deletes itself upon fade-out completion or
// whenever |Remove()| is explicitly called.
class TouchPointView : public views::View,
                       public ui::AnimationDelegate,
                       public views::WidgetObserver {
 public:
  explicit TouchPointView(views::Widget* parent_widget)
      : circle_center_(kPointRadius + 1, kPointRadius + 1),
        gradient_center_(SkPoint::Make(kPointRadius + 1,
                                       kPointRadius + 1)) {
    SetPaintToLayer(true);
    SetFillsBoundsOpaquely(false);

    SetSize(gfx::Size(2 * kPointRadius + 2, 2 * kPointRadius + 2));

    stroke_paint_.setStyle(SkPaint::kStroke_Style);
    stroke_paint_.setColor(kProjectionStrokeColor);

    gradient_colors_[0] = kProjectionFillColor;
    gradient_colors_[1] = kProjectionStrokeColor;

    gradient_pos_[0] = SkFloatToScalar(0.9f);
    gradient_pos_[1] = SkFloatToScalar(1.0f);

    parent_widget->GetContentsView()->AddChildView(this);

    parent_widget->AddObserver(this);
  }

  void UpdateTouch(const ui::TouchEvent& touch) {
    if (touch.type() == ui::ET_TOUCH_RELEASED ||
        touch.type() == ui::ET_TOUCH_CANCELLED) {
      fadeout_.reset(new ui::LinearAnimation(kFadeoutDurationInMs,
                                             kFadeoutFrameRate,
                                             this));
      fadeout_->Start();
    } else {
      SetX(touch.root_location().x() - kPointRadius - 1);
      SetY(touch.root_location().y() - kPointRadius - 1);
    }
  }

  void Remove() {
    delete this;
  }

 private:
  virtual ~TouchPointView() {
    GetWidget()->RemoveObserver(this);
    parent()->RemoveChildView(this);
  }

  // Overridden from views::View.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    int alpha = kProjectionAlpha;
    if (fadeout_)
      alpha = static_cast<int>(fadeout_->CurrentValueBetween(alpha, 0));
    fill_paint_.setAlpha(alpha);
    stroke_paint_.setAlpha(alpha);
    SkShader* shader = SkGradientShader::CreateRadial(
        gradient_center_,
        SkIntToScalar(kPointRadius),
        gradient_colors_,
        gradient_pos_,
        arraysize(gradient_colors_),
        SkShader::kMirror_TileMode,
        NULL);
    fill_paint_.setShader(shader);
    shader->unref();
    canvas->DrawCircle(circle_center_, SkIntToScalar(kPointRadius),
                       fill_paint_);
    canvas->DrawCircle(circle_center_, SkIntToScalar(kPointRadius),
                       stroke_paint_);
  }

  // Overridden from ui::AnimationDelegate.
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE {
    DCHECK_EQ(fadeout_.get(), animation);
    delete this;
  }

  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE {
    DCHECK_EQ(fadeout_.get(), animation);
    SchedulePaint();
  }

  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE {
    AnimationEnded(animation);
  }

  // Overridden from views::WidgetObserver.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE {
    fadeout_->Stop();
  }

  const gfx::Point circle_center_;
  const SkPoint gradient_center_;

  SkPaint fill_paint_;
  SkPaint stroke_paint_;
  SkColor gradient_colors_[2];
  SkScalar gradient_pos_[2];

  scoped_ptr<ui::Animation> fadeout_;

  DISALLOW_COPY_AND_ASSIGN(TouchPointView);
};

TouchObserverHUD::TouchObserverHUD(aura::RootWindow* initial_root)
    : display_id_(initial_root->GetProperty(kDisplayIdKey)),
      root_window_(initial_root),
      mode_(FULLSCREEN),
      touch_log_(new TouchLog()) {
  const gfx::Display& display =
      Shell::GetInstance()->display_manager()->GetDisplayForId(display_id_);

  views::View* content = new views::View;

  canvas_ = new TouchHudCanvas(*touch_log_);
  content->AddChildView(canvas_);

  const gfx::Size& display_size = display.size();
  canvas_->SetSize(display_size);
  content->SetSize(display_size);

  label_container_ = new views::View;
  label_container_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, 0));

  for (int i = 0; i < kMaxTouchPoints; ++i) {
    touch_labels_[i] = new views::Label;
    touch_labels_[i]->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
    touch_labels_[i]->SetShadowColors(SK_ColorWHITE,
                                      SK_ColorWHITE);
    touch_labels_[i]->SetShadowOffset(1, 1);
    label_container_->AddChildView(touch_labels_[i]);
  }
  label_container_->SetX(0);
  label_container_->SetY(display_size.height() / kReducedScale);
  label_container_->SetSize(label_container_->GetPreferredSize());
  label_container_->SetVisible(false);
  content->AddChildView(label_container_);

  widget_ = new views::Widget();
  views::Widget::InitParams
      params(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.transparent = true;
  params.can_activate = false;
  params.accept_events = false;
  params.bounds = gfx::Rect(display_size);
  params.parent = Shell::GetContainer(
      root_window_,
      internal::kShellWindowId_OverlayContainer);
  widget_->Init(params);
  widget_->SetContentsView(content);
  widget_->StackAtTop();
  widget_->Show();

  widget_->AddObserver(this);

  // Observe changes in display size and mode to update touch HUD.
  Shell::GetScreen()->AddObserver(this);
#if defined(OS_CHROMEOS)
  Shell::GetInstance()->output_configurator()->AddObserver(this);
#endif  // defined(OS_CHROMEOS)

  Shell::GetInstance()->display_controller()->AddObserver(this);
  root_window_->AddPreTargetHandler(this);
}

TouchObserverHUD::~TouchObserverHUD() {
  Shell::GetInstance()->display_controller()->RemoveObserver(this);

#if defined(OS_CHROMEOS)
  Shell::GetInstance()->output_configurator()->RemoveObserver(this);
#endif  // defined(OS_CHROMEOS)
  Shell::GetScreen()->RemoveObserver(this);

  widget_->RemoveObserver(this);
}

// static
scoped_ptr<DictionaryValue> TouchObserverHUD::GetAllAsDictionary() {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  Shell::RootWindowList roots = Shell::GetInstance()->GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = roots.begin();
      iter != roots.end(); ++iter) {
    internal::RootWindowController* controller = GetRootWindowController(*iter);
    if (controller->touch_observer_hud()) {
      int64 display_id = (*iter)->GetProperty(kDisplayIdKey);
      scoped_ptr<ListValue> list =
          controller->touch_observer_hud()->GetLogAsList();
      if (!list->empty())
        value->Set(base::Int64ToString(display_id), list.release());
    }
  }
  return value.Pass();
}

void TouchObserverHUD::ChangeToNextMode() {
  switch (mode_) {
    case FULLSCREEN:
      SetMode(REDUCED_SCALE);
      break;
    case REDUCED_SCALE:
      SetMode(PROJECTION);
      break;
    case PROJECTION:
      SetMode(INVISIBLE);
      break;
    case INVISIBLE:
      SetMode(FULLSCREEN);
      break;
  }
}

void TouchObserverHUD::Clear() {
  if (widget_->IsVisible())
    canvas_->Clear();
  for (int i = 0; i < kMaxTouchPoints; ++i)
    touch_labels_[i]->SetText(string16());
  label_container_->SetSize(label_container_->GetPreferredSize());
}

scoped_ptr<ListValue> TouchObserverHUD::GetLogAsList() const {
  return touch_log_->GetAsList();
}

void TouchObserverHUD::SetMode(Mode mode) {
  if (mode_ == mode)
    return;
  // When going out of projection mode, hide all active touch points.
  if (mode_ == PROJECTION) {
    for (std::map<int, TouchPointView*>::iterator iter = points_.begin();
        iter != points_.end(); ++iter)
      iter->second->Remove();
    points_.clear();
  }
  mode_ = mode;
  switch (mode) {
    case FULLSCREEN:
      label_container_->SetVisible(false);
      canvas_->SetVisible(true);
      canvas_->SetScale(1);
      canvas_->SchedulePaint();
      widget_->Show();
      break;
    case REDUCED_SCALE:
      label_container_->SetVisible(true);
      canvas_->SetVisible(true);
      canvas_->SetScale(kReducedScale);
      canvas_->SchedulePaint();
      widget_->Show();
      break;
    case PROJECTION:
      label_container_->SetVisible(false);
      canvas_->SetVisible(false);
      widget_->Show();
      break;
    case INVISIBLE:
      widget_->Hide();
      break;
  }
}

void TouchObserverHUD::UpdateTouchPointLabel(int index) {
  int trace_index = touch_log_->GetTraceIndex(index);
  const TouchTrace& trace = touch_log_->traces()[trace_index];
  TouchTrace::const_reverse_iterator point = trace.log().rbegin();
  ui::EventType touch_status = point->type;
  float touch_radius = std::max(point->radius_x, point->radius_y);
  while (point != trace.log().rend() && point->type == ui::ET_TOUCH_CANCELLED)
    point++;
  DCHECK(point != trace.log().rend());
  gfx::Point touch_position = point->location;

  std::string string = base::StringPrintf("%2d: %s %s (%.4f)",
                                          index,
                                          GetTouchEventLabel(touch_status),
                                          touch_position.ToString().c_str(),
                                          touch_radius);
  touch_labels_[index]->SetText(UTF8ToUTF16(string));
}

void TouchObserverHUD::OnTouchEvent(ui::TouchEvent* event) {
  if (event->touch_id() >= kMaxTouchPoints)
    return;

  touch_log_->AddTouchPoint(*event);
  canvas_->TouchPointAdded(event->touch_id());

  if (mode_ == PROJECTION) {
    if (event->type() == ui::ET_TOUCH_PRESSED) {
      TouchPointView* point = new TouchPointView(widget_);
      point->UpdateTouch(*event);
      std::pair<std::map<int, TouchPointView*>::iterator, bool> result =
          points_.insert(std::make_pair(event->touch_id(), point));
      // If a |TouchPointView| is already mapped to the touch id, remove it and
      // replace it with the new one.
      if (!result.second) {
        result.first->second->Remove();
        result.first->second = point;
      }
    } else {
      std::map<int, TouchPointView*>::iterator iter =
          points_.find(event->touch_id());
      if (iter != points_.end()) {
        iter->second->UpdateTouch(*event);
        if (event->type() == ui::ET_TOUCH_RELEASED ||
            event->type() == ui::ET_TOUCH_CANCELLED)
          points_.erase(iter);
      }
    }
  }

  UpdateTouchPointLabel(event->touch_id());
  label_container_->SetSize(label_container_->GetPreferredSize());
}

void TouchObserverHUD::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, widget_);
  delete this;
}

void TouchObserverHUD::OnDisplayBoundsChanged(const gfx::Display& display) {
  if (display.id() != display_id_)
    return;
  const gfx::Size& size = display.size();
  widget_->SetSize(size);
  canvas_->SetSize(size);
  label_container_->SetY(size.height() / kReducedScale);
}

void TouchObserverHUD::OnDisplayAdded(const gfx::Display& new_display) {}

void TouchObserverHUD::OnDisplayRemoved(const gfx::Display& old_display) {
  if (old_display.id() != display_id_)
    return;
  widget_->CloseNow();
}

#if defined(OS_CHROMEOS)
void TouchObserverHUD::OnDisplayModeChanged() {
  // Clear touch HUD for any change in display mode (single, dual extended, dual
  // mirrored, ...).
  Clear();
}
#endif  // defined(OS_CHROMEOS)

void TouchObserverHUD::OnDisplayConfigurationChanging() {
  if (!root_window_)
    return;

  root_window_->RemovePreTargetHandler(this);

  RootWindowController* controller = GetRootWindowController(root_window_);
  controller->set_touch_observer_hud(NULL);

  views::Widget::ReparentNativeView(
      widget_->GetNativeView(),
      Shell::GetContainer(root_window_,
                          internal::kShellWindowId_UnparentedControlContainer));

  root_window_ = NULL;
}

void TouchObserverHUD::OnDisplayConfigurationChanged() {
  if (root_window_)
    return;

  root_window_ = Shell::GetInstance()->display_controller()->
      GetRootWindowForDisplayId(display_id_);

  views::Widget::ReparentNativeView(
      widget_->GetNativeView(),
      Shell::GetContainer(root_window_,
                          internal::kShellWindowId_OverlayContainer));

  RootWindowController* controller = GetRootWindowController(root_window_);
  controller->set_touch_observer_hud(this);

  root_window_->AddPreTargetHandler(this);
}

}  // namespace internal
}  // namespace ash
