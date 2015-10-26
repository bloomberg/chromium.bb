// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_synthetic_delay.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/output/output_surface.h"
#include "cc/trees/layer_tree_host.h"
#include "components/scheduler/renderer/render_widget_scheduling_state.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "content/child/npapi/webplugin.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/input/synthetic_gesture_packet.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/common/input_messages.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/renderer/cursor_utils.h"
#include "content/renderer/external_popup_menu.h"
#include "content/renderer/gpu/compositor_output_surface.h"
#include "content/renderer/gpu/delegated_compositor_output_surface.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "content/renderer/gpu/mailbox_output_surface.h"
#include "content/renderer/gpu/queue_message_swap_promise.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/resizing_mode_selector.h"
#include "ipc/ipc_sync_message.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDeviceEmulationParams.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebPopupMenuInfo.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_switches.h"
#include "ui/surface/transport_dib.h"

#if defined(OS_ANDROID)
#include <android/keycodes.h>
#include "content/renderer/android/synchronous_compositor_factory.h"
#endif

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#endif  // defined(OS_POSIX)

#include "third_party/WebKit/public/web/WebWidget.h"

using blink::WebCompositionUnderline;
using blink::WebCursorInfo;
using blink::WebDeviceEmulationParams;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebNavigationPolicy;
using blink::WebNode;
using blink::WebPagePopup;
using blink::WebPoint;
using blink::WebPopupType;
using blink::WebRange;
using blink::WebRect;
using blink::WebScreenInfo;
using blink::WebSize;
using blink::WebTextDirection;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using blink::WebVector;
using blink::WebWidget;

namespace {

typedef std::map<std::string, ui::TextInputMode> TextInputModeMap;

class TextInputModeMapSingleton {
 public:
  static TextInputModeMapSingleton* GetInstance() {
    return base::Singleton<TextInputModeMapSingleton>::get();
  }
  TextInputModeMapSingleton() {
    map_["verbatim"] = ui::TEXT_INPUT_MODE_VERBATIM;
    map_["latin"] = ui::TEXT_INPUT_MODE_LATIN;
    map_["latin-name"] = ui::TEXT_INPUT_MODE_LATIN_NAME;
    map_["latin-prose"] = ui::TEXT_INPUT_MODE_LATIN_PROSE;
    map_["full-width-latin"] = ui::TEXT_INPUT_MODE_FULL_WIDTH_LATIN;
    map_["kana"] = ui::TEXT_INPUT_MODE_KANA;
    map_["katakana"] = ui::TEXT_INPUT_MODE_KATAKANA;
    map_["numeric"] = ui::TEXT_INPUT_MODE_NUMERIC;
    map_["tel"] = ui::TEXT_INPUT_MODE_TEL;
    map_["email"] = ui::TEXT_INPUT_MODE_EMAIL;
    map_["url"] = ui::TEXT_INPUT_MODE_URL;
  }
  const TextInputModeMap& map() const { return map_; }
 private:
  TextInputModeMap map_;

  friend struct base::DefaultSingletonTraits<TextInputModeMapSingleton>;

  DISALLOW_COPY_AND_ASSIGN(TextInputModeMapSingleton);
};

ui::TextInputMode ConvertInputMode(const blink::WebString& input_mode) {
  static TextInputModeMapSingleton* singleton =
      TextInputModeMapSingleton::GetInstance();
  TextInputModeMap::const_iterator it =
      singleton->map().find(input_mode.utf8());
  if (it == singleton->map().end())
    return ui::TEXT_INPUT_MODE_DEFAULT;
  return it->second;
}

// TODO(brianderson): Replace the hard-coded threshold with a fraction of
// the BeginMainFrame interval.
// 4166us will allow 1/4 of a 60Hz interval or 1/2 of a 120Hz interval to
// be spent in input hanlders before input starts getting throttled.
const int kInputHandlingTimeThrottlingThresholdMicroseconds = 4166;

int64 GetEventLatencyMicros(double event_timestamp, base::TimeTicks now) {
  return (now - base::TimeDelta::FromSecondsD(event_timestamp))
      .ToInternalValue();
}

void LogInputEventLatencyUmaImpl(WebInputEvent::Type event_type,
                                 double event_timestamp,
                                 base::TimeTicks now) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Event.AggregatedLatency.Renderer2",
                              GetEventLatencyMicros(event_timestamp, now), 1,
                              10000000, 100);

#define CASE_TYPE(t)                                                         \
  case WebInputEvent::t:                                                     \
    UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.Renderer2." #t,               \
                                GetEventLatencyMicros(event_timestamp, now), \
                                1, 10000000, 100);                           \
    break;

  switch (event_type) {
    CASE_TYPE(Undefined);
    CASE_TYPE(MouseDown);
    CASE_TYPE(MouseUp);
    CASE_TYPE(MouseMove);
    CASE_TYPE(MouseEnter);
    CASE_TYPE(MouseLeave);
    CASE_TYPE(ContextMenu);
    CASE_TYPE(MouseWheel);
    CASE_TYPE(RawKeyDown);
    CASE_TYPE(KeyDown);
    CASE_TYPE(KeyUp);
    CASE_TYPE(Char);
    CASE_TYPE(GestureScrollBegin);
    CASE_TYPE(GestureScrollEnd);
    CASE_TYPE(GestureScrollUpdate);
    CASE_TYPE(GestureFlingStart);
    CASE_TYPE(GestureFlingCancel);
    CASE_TYPE(GestureShowPress);
    CASE_TYPE(GestureTap);
    CASE_TYPE(GestureTapUnconfirmed);
    CASE_TYPE(GestureTapDown);
    CASE_TYPE(GestureTapCancel);
    CASE_TYPE(GestureDoubleTap);
    CASE_TYPE(GestureTwoFingerTap);
    CASE_TYPE(GestureLongPress);
    CASE_TYPE(GestureLongTap);
    CASE_TYPE(GesturePinchBegin);
    CASE_TYPE(GesturePinchEnd);
    CASE_TYPE(GesturePinchUpdate);
    CASE_TYPE(TouchStart);
    CASE_TYPE(TouchMove);
    CASE_TYPE(TouchEnd);
    CASE_TYPE(TouchCancel);
    default:
      // Must include default to let blink::WebInputEvent add new event types
      // before they're added here.
      DLOG(WARNING) << "Unhandled WebInputEvent type: " << event_type;
      break;
  }

#undef CASE_TYPE
}

void LogInputEventLatencyUma(const WebInputEvent& event, base::TimeTicks now,
                             const ui::LatencyInfo& latency_info) {
  LogInputEventLatencyUmaImpl(event.type, event.timeStampSeconds, now);
  for (size_t i = 0; i < latency_info.coalesced_events_size(); i++) {
    LogInputEventLatencyUmaImpl(
        event.type,
        latency_info.timestamps_of_coalesced_events()[i],
        now);
  }
}

}  // namespace

namespace content {

// RenderWidget::ScreenMetricsEmulator ----------------------------------------

class RenderWidget::ScreenMetricsEmulator {
 public:
  ScreenMetricsEmulator(
      RenderWidget* widget,
      const WebDeviceEmulationParams& params);
  virtual ~ScreenMetricsEmulator();

  // Scale and offset used to convert between host coordinates
  // and webwidget coordinates.
  float scale() { return scale_; }
  gfx::PointF offset() { return offset_; }
  gfx::Rect applied_widget_rect() const { return applied_widget_rect_; }
  gfx::Rect original_screen_rect() const { return original_view_screen_rect_; }
  const WebScreenInfo& original_screen_info() { return original_screen_info_; }

  void ChangeEmulationParams(
      const WebDeviceEmulationParams& params);

  // The following methods alter handlers' behavior for messages related to
  // widget size and position.
  void OnResizeMessage(const ViewMsg_Resize_Params& params);
  void OnUpdateScreenRectsMessage(const gfx::Rect& view_screen_rect,
                                  const gfx::Rect& window_screen_rect);
  void OnShowContextMenu(ContextMenuParams* params);
  gfx::Rect AdjustValidationMessageAnchor(const gfx::Rect& anchor);

 private:
  void Reapply();
  void Apply(bool top_controls_shrink_blink_size,
             float top_controls_height,
             gfx::Rect resizer_rect,
             bool is_fullscreen_granted,
             blink::WebDisplayMode display_mode);

  RenderWidget* widget_;

  // Parameters as passed by RenderWidget::EnableScreenMetricsEmulation.
  WebDeviceEmulationParams params_;

  // The computed scale and offset used to fit widget into browser window.
  float scale_;
  gfx::PointF offset_;

  // Widget rect as passed to webwidget.
  gfx::Rect applied_widget_rect_;

  // Original values to restore back after emulation ends.
  gfx::Size original_size_;
  gfx::Size original_physical_backing_size_;
  gfx::Size original_visible_viewport_size_;
  blink::WebScreenInfo original_screen_info_;
  gfx::Rect original_view_screen_rect_;
  gfx::Rect original_window_screen_rect_;
};

RenderWidget::ScreenMetricsEmulator::ScreenMetricsEmulator(
    RenderWidget* widget,
    const WebDeviceEmulationParams& params)
    : widget_(widget),
      params_(params),
      scale_(1.f) {
  original_size_ = widget_->size_;
  original_physical_backing_size_ = widget_->physical_backing_size_;
  original_visible_viewport_size_ = widget_->visible_viewport_size_;
  original_screen_info_ = widget_->screen_info_;
  original_view_screen_rect_ = widget_->view_screen_rect_;
  original_window_screen_rect_ = widget_->window_screen_rect_;
  Apply(widget_->top_controls_shrink_blink_size_,
        widget_->top_controls_height_,
        widget_->resizer_rect_,
        widget_->is_fullscreen_granted_,
        widget_->display_mode_);
}

RenderWidget::ScreenMetricsEmulator::~ScreenMetricsEmulator() {
  widget_->screen_info_ = original_screen_info_;

  widget_->SetDeviceScaleFactor(original_screen_info_.deviceScaleFactor);
  widget_->SetScreenMetricsEmulationParameters(false, params_);
  widget_->view_screen_rect_ = original_view_screen_rect_;
  widget_->window_screen_rect_ = original_window_screen_rect_;
  widget_->Resize(original_size_,
                  original_physical_backing_size_,
                  widget_->top_controls_shrink_blink_size_,
                  widget_->top_controls_height_,
                  original_visible_viewport_size_,
                  widget_->resizer_rect_,
                  widget_->is_fullscreen_granted_,
                  widget_->display_mode_,
                  NO_RESIZE_ACK);
}

void RenderWidget::ScreenMetricsEmulator::ChangeEmulationParams(
    const WebDeviceEmulationParams& params) {
  params_ = params;
  Reapply();
}

void RenderWidget::ScreenMetricsEmulator::Reapply() {
  Apply(widget_->top_controls_shrink_blink_size_,
        widget_->top_controls_height_,
        widget_->resizer_rect_,
        widget_->is_fullscreen_granted_,
        widget_->display_mode_);
}

void RenderWidget::ScreenMetricsEmulator::Apply(
    bool top_controls_shrink_blink_size,
    float top_controls_height,
    gfx::Rect resizer_rect,
    bool is_fullscreen_granted,
    blink::WebDisplayMode display_mode) {
  applied_widget_rect_.set_size(gfx::Size(params_.viewSize));
  if (!applied_widget_rect_.width())
    applied_widget_rect_.set_width(original_size_.width());
  if (!applied_widget_rect_.height())
    applied_widget_rect_.set_height(original_size_.height());

  if (params_.fitToView && !original_size_.IsEmpty()) {
    int original_width = std::max(original_size_.width(), 1);
    int original_height = std::max(original_size_.height(), 1);
    float width_ratio =
        static_cast<float>(applied_widget_rect_.width()) / original_width;
    float height_ratio =
        static_cast<float>(applied_widget_rect_.height()) / original_height;
    float ratio = std::max(1.0f, std::max(width_ratio, height_ratio));
    scale_ = 1.f / ratio;

    // Center emulated view inside available view space.
    offset_.set_x(
        (original_size_.width() - scale_ * applied_widget_rect_.width()) / 2);
    offset_.set_y(
        (original_size_.height() - scale_ * applied_widget_rect_.height()) / 2);
  } else {
    scale_ = params_.scale;
    offset_.SetPoint(params_.offset.x, params_.offset.y);
  }

  if (params_.screenPosition == WebDeviceEmulationParams::Desktop) {
    applied_widget_rect_.set_origin(original_view_screen_rect_.origin());
    widget_->screen_info_.rect = original_screen_info_.rect;
    widget_->screen_info_.availableRect = original_screen_info_.availableRect;
    widget_->window_screen_rect_ = original_window_screen_rect_;
  } else {
    applied_widget_rect_.set_origin(params_.viewPosition);
    gfx::Rect screen_rect = applied_widget_rect_;
    if (!params_.screenSize.isEmpty()) {
      screen_rect =
          gfx::Rect(0, 0, params_.screenSize.width, params_.screenSize.height);
    }
    widget_->screen_info_.rect = screen_rect;
    widget_->screen_info_.availableRect = screen_rect;
    widget_->window_screen_rect_ = applied_widget_rect_;
  }

  float applied_device_scale_factor = params_.deviceScaleFactor ?
      params_.deviceScaleFactor : original_screen_info_.deviceScaleFactor;
  widget_->screen_info_.deviceScaleFactor = applied_device_scale_factor;

  // Pass three emulation parameters to the blink side:
  // - we keep the real device scale factor in compositor to produce sharp image
  //   even when emulating different scale factor;
  // - in order to fit into view, WebView applies offset and scale to the
  //   root layer.
  blink::WebDeviceEmulationParams modified_params = params_;
  modified_params.deviceScaleFactor = original_screen_info_.deviceScaleFactor;
  modified_params.offset = blink::WebFloatPoint(offset_.x(), offset_.y());
  modified_params.scale = scale_;
  widget_->SetScreenMetricsEmulationParameters(true, modified_params);

  widget_->SetDeviceScaleFactor(applied_device_scale_factor);
  widget_->view_screen_rect_ = applied_widget_rect_;

  gfx::Size physical_backing_size = gfx::ScaleToCeiledSize(
      original_size_, original_screen_info_.deviceScaleFactor);
  widget_->Resize(applied_widget_rect_.size(),
                  physical_backing_size,
                  top_controls_shrink_blink_size,
                  top_controls_height,
                  applied_widget_rect_.size(),
                  resizer_rect,
                  is_fullscreen_granted,
                  display_mode,
                  NO_RESIZE_ACK);
}

void RenderWidget::ScreenMetricsEmulator::OnResizeMessage(
    const ViewMsg_Resize_Params& params) {
  bool need_ack = params.new_size != original_size_ &&
      !params.new_size.IsEmpty() && !params.physical_backing_size.IsEmpty();
  original_size_ = params.new_size;
  original_physical_backing_size_ = params.physical_backing_size;
  original_screen_info_ = params.screen_info;
  original_visible_viewport_size_ = params.visible_viewport_size;
  Apply(params.top_controls_shrink_blink_size,
        params.top_controls_height,
        params.resizer_rect,
        params.is_fullscreen_granted,
        params.display_mode);

  if (need_ack) {
    widget_->set_next_paint_is_resize_ack();
    if (widget_->compositor_)
      widget_->compositor_->SetNeedsRedrawRect(gfx::Rect(widget_->size_));
  }
}

void RenderWidget::ScreenMetricsEmulator::OnUpdateScreenRectsMessage(
    const gfx::Rect& view_screen_rect,
    const gfx::Rect& window_screen_rect) {
  original_view_screen_rect_ = view_screen_rect;
  original_window_screen_rect_ = window_screen_rect;
  if (params_.screenPosition == WebDeviceEmulationParams::Desktop)
    Reapply();
}

void RenderWidget::ScreenMetricsEmulator::OnShowContextMenu(
    ContextMenuParams* params) {
  params->x *= scale_;
  params->x += offset_.x();
  params->y *= scale_;
  params->y += offset_.y();
}

gfx::Rect RenderWidget::ScreenMetricsEmulator::AdjustValidationMessageAnchor(
    const gfx::Rect& anchor) {
  gfx::Rect scaled = gfx::ScaleToEnclosedRect(anchor, scale_);
  scaled.set_x(scaled.x() + offset_.x());
  scaled.set_y(scaled.y() + offset_.y());
  return scaled;
}

// RenderWidget ---------------------------------------------------------------

RenderWidget::RenderWidget(CompositorDependencies* compositor_deps,
                           blink::WebPopupType popup_type,
                           const blink::WebScreenInfo& screen_info,
                           bool swapped_out,
                           bool hidden,
                           bool never_visible)
    : routing_id_(MSG_ROUTING_NONE),
      compositor_deps_(compositor_deps),
      webwidget_(nullptr),
      opener_id_(MSG_ROUTING_NONE),
      top_controls_shrink_blink_size_(false),
      top_controls_height_(0.f),
      next_paint_flags_(0),
      auto_resize_mode_(false),
      need_update_rect_for_auto_resize_(false),
      did_show_(false),
      is_hidden_(hidden),
      compositor_never_visible_(never_visible),
      is_fullscreen_granted_(false),
      display_mode_(blink::WebDisplayModeUndefined),
      handling_input_event_(false),
      handling_event_overscroll_(nullptr),
      handling_ime_event_(false),
      handling_event_type_(WebInputEvent::Undefined),
      ignore_ack_for_mouse_move_from_debugger_(false),
      closing_(false),
      host_closing_(false),
      is_swapped_out_(swapped_out),
      for_oopif_(false),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      text_input_mode_(ui::TEXT_INPUT_MODE_DEFAULT),
      text_input_flags_(0),
      can_compose_inline_(true),
      popup_type_(popup_type),
      pending_window_rect_count_(0),
      suppress_next_char_events_(false),
      screen_info_(screen_info),
      device_scale_factor_(screen_info_.deviceScaleFactor),
      next_output_surface_id_(0),
#if defined(OS_ANDROID)
      text_field_is_dirty_(false),
#endif
      popup_origin_scale_for_emulation_(0.f),
      frame_swap_message_queue_(new FrameSwapMessageQueue()),
      resizing_mode_selector_(new ResizingModeSelector()),
      context_menu_source_type_(ui::MENU_SOURCE_MOUSE),
      has_host_context_menu_location_(false) {
  if (!swapped_out)
    RenderProcess::current()->AddRefProcess();
  DCHECK(RenderThread::Get());
  device_color_profile_.push_back('0');
#if defined(OS_ANDROID)
  text_input_info_history_.push_back(blink::WebTextInputInfo());
#endif

  // In tests there may not be a RenderThreadImpl.
  if (RenderThreadImpl::current()) {
    render_widget_scheduling_state_ = RenderThreadImpl::current()
                                          ->GetRendererScheduler()
                                          ->NewRenderWidgetSchedulingState()
                                          .Pass();
    render_widget_scheduling_state_->SetHidden(is_hidden_);
  }
}

RenderWidget::~RenderWidget() {
  DCHECK(!webwidget_) << "Leaking our WebWidget!";

  // If we are swapped out, we have released already.
  if (!is_swapped_out_ && RenderProcess::current())
    RenderProcess::current()->ReleaseProcess();
}

// static
RenderWidget* RenderWidget::Create(int32 opener_id,
                                   CompositorDependencies* compositor_deps,
                                   blink::WebPopupType popup_type,
                                   const blink::WebScreenInfo& screen_info) {
  DCHECK(opener_id != MSG_ROUTING_NONE);
  scoped_refptr<RenderWidget> widget(new RenderWidget(
      compositor_deps, popup_type, screen_info, false, false, false));
  if (widget->Init(opener_id)) {  // adds reference on success.
    return widget.get();
  }
  return NULL;
}

// static
RenderWidget* RenderWidget::CreateForFrame(
    int routing_id,
    bool hidden,
    const blink::WebScreenInfo& screen_info,
    CompositorDependencies* compositor_deps,
    blink::WebLocalFrame* frame) {
  CHECK_NE(routing_id, MSG_ROUTING_NONE);
  // TODO(avi): Before RenderViewImpl has-a RenderWidget, the browser passes the
  // same routing ID for both the view routing ID and the main frame widget
  // routing ID. https://crbug.com/545684
  RenderViewImpl* view = RenderViewImpl::FromRoutingID(routing_id);
  if (view) {
    view->AttachWebFrameWidget(RenderWidget::CreateWebFrameWidget(view, frame));
    return view;
  }
  scoped_refptr<RenderWidget> widget(
      new RenderWidget(compositor_deps, blink::WebPopupTypeNone, screen_info,
                       false, hidden, false));
  widget->routing_id_ = routing_id;
  widget->for_oopif_ = true;
  // DoInit increments the reference count on |widget|, keeping it alive after
  // this function returns.
  if (widget->DoInit(MSG_ROUTING_NONE,
                     RenderWidget::CreateWebFrameWidget(widget.get(), frame),
                     nullptr)) {
    return widget.get();
  }
  return nullptr;
}

// static
blink::WebWidget* RenderWidget::CreateWebFrameWidget(
    RenderWidget* render_widget,
    blink::WebLocalFrame* frame) {
  if (!frame->parent()) {
    // TODO(dcheng): The main frame widget currently has a special case.
    // Eliminate this once WebView is no longer a WebWidget.
    return blink::WebFrameWidget::create(render_widget, frame->view(), frame);
  }
  return blink::WebFrameWidget::create(render_widget, frame);
}

// static
blink::WebWidget* RenderWidget::CreateWebWidget(RenderWidget* render_widget) {
  switch (render_widget->popup_type_) {
    case blink::WebPopupTypeNone:  // Nothing to create.
      break;
    case blink::WebPopupTypePage:
      return WebPagePopup::create(render_widget);
    default:
      NOTREACHED();
  }
  return NULL;
}

void RenderWidget::CloseForFrame() {
  OnClose();
}

bool RenderWidget::Init(int32 opener_id) {
  return DoInit(
      opener_id, RenderWidget::CreateWebWidget(this),
      new ViewHostMsg_CreateWidget(opener_id, popup_type_, &routing_id_));
}

bool RenderWidget::DoInit(int32 opener_id,
                          WebWidget* web_widget,
                          IPC::SyncMessage* create_widget_message) {
  DCHECK(!webwidget_);

  if (opener_id != MSG_ROUTING_NONE)
    opener_id_ = opener_id;

  webwidget_ = web_widget;

  bool result = true;
  if (create_widget_message)
    result = RenderThread::Get()->Send(create_widget_message);

  if (result) {
    RenderThread::Get()->AddRoute(routing_id_, this);
    // Take a reference on behalf of the RenderThread.  This will be balanced
    // when we receive ViewMsg_Close.
    AddRef();
    if (RenderThreadImpl::current()) {
      RenderThreadImpl::current()->WidgetCreated();
      if (is_hidden_)
        RenderThreadImpl::current()->WidgetHidden();
    }
    return true;
  } else {
    // The above Send can fail when the tab is closing.
    return false;
  }
}

void RenderWidget::SetSwappedOut(bool is_swapped_out) {
  // We should only toggle between states.
  DCHECK(is_swapped_out_ != is_swapped_out);
  is_swapped_out_ = is_swapped_out;

  // If we are swapping out, we will call ReleaseProcess, allowing the process
  // to exit if all of its RenderViews are swapped out.  We wait until the
  // WasSwappedOut call to do this, to allow the unload handler to finish.
  // If we are swapping in, we call AddRefProcess to prevent the process from
  // exiting.
  if (!is_swapped_out_)
    RenderProcess::current()->AddRefProcess();
}

void RenderWidget::WasSwappedOut() {
  // If we have been swapped out and no one else is using this process,
  // it's safe to exit now.
  CHECK(is_swapped_out_);
  RenderProcess::current()->ReleaseProcess();
}

void RenderWidget::SetPopupOriginAdjustmentsForEmulation(
    ScreenMetricsEmulator* emulator) {
  popup_origin_scale_for_emulation_ = emulator->scale();
  popup_view_origin_for_emulation_ = emulator->applied_widget_rect().origin();
  popup_screen_origin_for_emulation_ = gfx::Point(
      emulator->original_screen_rect().origin().x() + emulator->offset().x(),
      emulator->original_screen_rect().origin().y() + emulator->offset().y());
  screen_info_ = emulator->original_screen_info();
  device_scale_factor_ = screen_info_.deviceScaleFactor;
}

gfx::Rect RenderWidget::AdjustValidationMessageAnchor(const gfx::Rect& anchor) {
  if (screen_metrics_emulator_)
    return screen_metrics_emulator_->AdjustValidationMessageAnchor(anchor);
  return anchor;
}

void RenderWidget::SetScreenMetricsEmulationParameters(
    bool enabled,
    const blink::WebDeviceEmulationParams& params) {
  // This is only supported in RenderView.
  NOTREACHED();
}

#if defined(OS_MACOSX) || defined(OS_ANDROID)
void RenderWidget::SetExternalPopupOriginAdjustmentsForEmulation(
    ExternalPopupMenu* popup, ScreenMetricsEmulator* emulator) {
  popup->SetOriginScaleAndOffsetForEmulation(
      emulator->scale(), emulator->offset());
}
#endif

void RenderWidget::OnShowHostContextMenu(ContextMenuParams* params) {
  if (screen_metrics_emulator_)
    screen_metrics_emulator_->OnShowContextMenu(params);
}

bool RenderWidget::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidget, message)
    IPC_MESSAGE_HANDLER(InputMsg_HandleInputEvent, OnHandleInputEvent)
    IPC_MESSAGE_HANDLER(InputMsg_CursorVisibilityChange,
                        OnCursorVisibilityChange)
    IPC_MESSAGE_HANDLER(InputMsg_ImeSetComposition, OnImeSetComposition)
    IPC_MESSAGE_HANDLER(InputMsg_ImeConfirmComposition, OnImeConfirmComposition)
    IPC_MESSAGE_HANDLER(InputMsg_MouseCaptureLost, OnMouseCaptureLost)
    IPC_MESSAGE_HANDLER(InputMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(InputMsg_SyntheticGestureCompleted,
                        OnSyntheticGestureCompleted)
    IPC_MESSAGE_HANDLER(ViewMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(ViewMsg_Resize, OnResize)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableDeviceEmulation,
                        OnEnableDeviceEmulation)
    IPC_MESSAGE_HANDLER(ViewMsg_DisableDeviceEmulation,
                        OnDisableDeviceEmulation)
    IPC_MESSAGE_HANDLER(ViewMsg_ColorProfile, OnColorProfile)
    IPC_MESSAGE_HANDLER(ViewMsg_ChangeResizeRect, OnChangeResizeRect)
    IPC_MESSAGE_HANDLER(ViewMsg_WasHidden, OnWasHidden)
    IPC_MESSAGE_HANDLER(ViewMsg_WasShown, OnWasShown)
    IPC_MESSAGE_HANDLER(ViewMsg_Repaint, OnRepaint)
    IPC_MESSAGE_HANDLER(ViewMsg_SetTextDirection, OnSetTextDirection)
    IPC_MESSAGE_HANDLER(ViewMsg_Move_ACK, OnRequestMoveAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateScreenRects, OnUpdateScreenRects)
    IPC_MESSAGE_HANDLER(ViewMsg_SetSurfaceIdNamespace, OnSetSurfaceIdNamespace)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(InputMsg_ImeEventAck, OnImeEventAck)
    IPC_MESSAGE_HANDLER(ViewMsg_ShowImeIfNeeded, OnShowImeIfNeeded)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool RenderWidget::Send(IPC::Message* message) {
  // Don't send any messages after the browser has told us to close, and filter
  // most outgoing messages while swapped out.
  if ((is_swapped_out_ &&
       !SwappedOutMessages::CanSendWhileSwappedOut(message)) ||
      closing_) {
    delete message;
    return false;
  }

  // If given a messsage without a routing ID, then assign our routing ID.
  if (message->routing_id() == MSG_ROUTING_NONE)
    message->set_routing_id(routing_id_);

  return RenderThread::Get()->Send(message);
}

void RenderWidget::Resize(const gfx::Size& new_size,
                          const gfx::Size& physical_backing_size,
                          bool top_controls_shrink_blink_size,
                          float top_controls_height,
                          const gfx::Size& visible_viewport_size,
                          const gfx::Rect& resizer_rect,
                          bool is_fullscreen_granted,
                          blink::WebDisplayMode display_mode,
                          const ResizeAck resize_ack) {
  if (resizing_mode_selector_->NeverUsesSynchronousResize()) {
    // A resize ack shouldn't be requested if we have not ACK'd the previous
    // one.
    DCHECK(resize_ack != SEND_RESIZE_ACK || !next_paint_is_resize_ack());
    DCHECK(resize_ack == SEND_RESIZE_ACK || resize_ack == NO_RESIZE_ACK);
  }

  // Ignore this during shutdown.
  if (!webwidget_)
    return;

  if (compositor_) {
    compositor_->setViewportSize(new_size, physical_backing_size);
  }

  physical_backing_size_ = physical_backing_size;
  top_controls_shrink_blink_size_ = top_controls_shrink_blink_size;
  top_controls_height_ = top_controls_height;
  visible_viewport_size_ = visible_viewport_size;
  resizer_rect_ = resizer_rect;

  // NOTE: We may have entered fullscreen mode without changing our size.
  bool fullscreen_change = is_fullscreen_granted_ != is_fullscreen_granted;
  is_fullscreen_granted_ = is_fullscreen_granted;
  display_mode_ = display_mode;

  webwidget_->setTopControlsHeight(top_controls_height,
                                   top_controls_shrink_blink_size_);

  if (size_ != new_size) {
    size_ = new_size;

    // When resizing, we want to wait to paint before ACK'ing the resize.  This
    // ensures that we only resize as fast as we can paint.  We only need to
    // send an ACK if we are resized to a non-empty rect.
    webwidget_->resize(new_size);
  }

  webwidget()->resizePinchViewport(gfx::Size(
      visible_viewport_size.width(),
      visible_viewport_size.height()));

  if (new_size.IsEmpty() || physical_backing_size.IsEmpty()) {
    // In this case there is no paint/composite and therefore no
    // ViewHostMsg_UpdateRect to send the resize ack with. We'd need to send the
    // ack through a fake ViewHostMsg_UpdateRect or a different message.
    DCHECK_EQ(resize_ack, NO_RESIZE_ACK);
  }

  // Send the Resize_ACK flag once we paint again if requested.
  if (resize_ack == SEND_RESIZE_ACK)
    set_next_paint_is_resize_ack();

  if (fullscreen_change)
    DidToggleFullscreen();

  // If a resize ack is requested and it isn't set-up, then no more resizes will
  // come in and in general things will go wrong.
  DCHECK(resize_ack != SEND_RESIZE_ACK || next_paint_is_resize_ack());
}

void RenderWidget::SetWindowRectSynchronously(
    const gfx::Rect& new_window_rect) {
  Resize(new_window_rect.size(),
         new_window_rect.size(),
         top_controls_shrink_blink_size_,
         top_controls_height_,
         new_window_rect.size(),
         gfx::Rect(),
         is_fullscreen_granted_,
         display_mode_,
         NO_RESIZE_ACK);
  view_screen_rect_ = new_window_rect;
  window_screen_rect_ = new_window_rect;
  if (!did_show_)
    initial_rect_ = new_window_rect;
}

void RenderWidget::OnClose() {
  DCHECK(content::RenderThread::Get());
  if (closing_)
    return;
  NotifyOnClose();
  closing_ = true;

  // Browser correspondence is no longer needed at this point.
  if (routing_id_ != MSG_ROUTING_NONE) {
    RenderThread::Get()->RemoveRoute(routing_id_);
    SetHidden(false);
    if (RenderThreadImpl::current())
      RenderThreadImpl::current()->WidgetDestroyed();
  }

  if (for_oopif_) {
    // Widgets for frames may be created and closed at any time while the frame
    // is alive. However, the closing process must happen synchronously. Frame
    // widget and frames hold pointers to each other. If Close() is deferred to
    // the message loop like in the non-frame widget case, WebWidget::close()
    // can end up accessing members of an already-deleted frame.
    Close();
  } else {
    // If there is a Send call on the stack, then it could be dangerous to close
    // now.  Post a task that only gets invoked when there are no nested message
    // loops.
    base::ThreadTaskRunnerHandle::Get()->PostNonNestableTask(
        FROM_HERE, base::Bind(&RenderWidget::Close, this));
  }

  // Balances the AddRef taken when we called AddRoute.
  Release();
}

void RenderWidget::OnResize(const ViewMsg_Resize_Params& params) {
  if (resizing_mode_selector_->ShouldAbortOnResize(this, params))
    return;

  if (screen_metrics_emulator_) {
    screen_metrics_emulator_->OnResizeMessage(params);
    return;
  }

  bool orientation_changed =
      screen_info_.orientationAngle != params.screen_info.orientationAngle;

  screen_info_ = params.screen_info;
  SetDeviceScaleFactor(screen_info_.deviceScaleFactor);
  Resize(params.new_size,
         params.physical_backing_size,
         params.top_controls_shrink_blink_size,
         params.top_controls_height,
         params.visible_viewport_size,
         params.resizer_rect,
         params.is_fullscreen_granted,
         params.display_mode,
         params.needs_resize_ack ? SEND_RESIZE_ACK : NO_RESIZE_ACK);

  if (orientation_changed)
    OnOrientationChange();
}

void RenderWidget::OnEnableDeviceEmulation(
   const blink::WebDeviceEmulationParams& params) {
  if (!screen_metrics_emulator_)
    screen_metrics_emulator_.reset(new ScreenMetricsEmulator(this, params));
  else
    screen_metrics_emulator_->ChangeEmulationParams(params);
}

void RenderWidget::OnDisableDeviceEmulation() {
  screen_metrics_emulator_.reset();
}

void RenderWidget::OnColorProfile(const std::vector<char>& color_profile) {
  SetDeviceColorProfile(color_profile);
}

void RenderWidget::OnChangeResizeRect(const gfx::Rect& resizer_rect) {
  if (resizer_rect_ == resizer_rect)
    return;
  resizer_rect_ = resizer_rect;
  if (webwidget_)
    webwidget_->didChangeWindowResizerRect();
}

void RenderWidget::OnWasHidden() {
  TRACE_EVENT0("renderer", "RenderWidget::OnWasHidden");
  // Go into a mode where we stop generating paint and scrolling events.
  SetHidden(true);
  FOR_EACH_OBSERVER(RenderFrameImpl, render_frames_,
                    WasHidden());
}

void RenderWidget::OnWasShown(bool needs_repainting,
                              const ui::LatencyInfo& latency_info) {
  TRACE_EVENT0("renderer", "RenderWidget::OnWasShown");
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  // See OnWasHidden
  SetHidden(false);
  FOR_EACH_OBSERVER(RenderFrameImpl, render_frames_,
                    WasShown());

  if (!needs_repainting)
    return;

  // Generate a full repaint.
  if (compositor_) {
    ui::LatencyInfo swap_latency_info(latency_info);
    scoped_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor(
        compositor_->CreateLatencyInfoSwapPromiseMonitor(&swap_latency_info));
    compositor_->SetNeedsForcedRedraw();
  }
  ScheduleComposite();
}

void RenderWidget::OnRequestMoveAck() {
  DCHECK(pending_window_rect_count_);
  pending_window_rect_count_--;
}

GURL RenderWidget::GetURLForGraphicsContext3D() {
  return GURL();
}

scoped_ptr<cc::OutputSurface> RenderWidget::CreateOutputSurface(bool fallback) {
  // For widgets that are never visible, we don't start the compositor, so we
  // never get a request for a cc::OutputSurface.
  DCHECK(!compositor_never_visible_);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  bool use_software = fallback;
  if (command_line.HasSwitch(switches::kDisableGpuCompositing))
    use_software = true;

  scoped_refptr<ContextProviderCommandBuffer> context_provider;
  scoped_refptr<ContextProviderCommandBuffer> worker_context_provider;
  if (!use_software) {
    context_provider = ContextProviderCommandBuffer::Create(
        CreateGraphicsContext3D(true), RENDER_COMPOSITOR_CONTEXT);
    if (!context_provider.get()) {
      // Cause the compositor to wait and try again.
      return nullptr;
    }
    worker_context_provider =
        RenderThreadImpl::current()->SharedWorkerContextProvider();
    if (!worker_context_provider) {
      // Cause the compositor to wait and try again.
      return nullptr;
    }

#if defined(OS_ANDROID)
    if (SynchronousCompositorFactory* factory =
            SynchronousCompositorFactory::GetInstance()) {
      return factory->CreateOutputSurface(
          routing_id(), frame_swap_message_queue_, context_provider,
          worker_context_provider);
    }
#endif
  }

  uint32 output_surface_id = next_output_surface_id_++;
  // Composite-to-mailbox is currently used for layout tests in order to cause
  // them to draw inside in the renderer to do the readback there. This should
  // no longer be the case when crbug.com/311404 is fixed.
  if (!RenderThreadImpl::current() ||
      !RenderThreadImpl::current()->layout_test_mode()) {
    DCHECK(compositor_deps_->GetCompositorImplThreadTaskRunner());
    return make_scoped_ptr(new DelegatedCompositorOutputSurface(
        routing_id(), output_surface_id, context_provider,
        worker_context_provider, frame_swap_message_queue_));
  }

  if (!context_provider.get()) {
    scoped_ptr<cc::SoftwareOutputDevice> software_device(
        new cc::SoftwareOutputDevice());

    return make_scoped_ptr(new CompositorOutputSurface(
        routing_id(), output_surface_id, nullptr, nullptr,
        software_device.Pass(), frame_swap_message_queue_, true));
  }

  return make_scoped_ptr(new MailboxOutputSurface(
      routing_id(), output_surface_id, context_provider,
      worker_context_provider, frame_swap_message_queue_, cc::RGBA_8888));
}

void RenderWidget::OnSwapBuffersAborted() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersAborted");
  // Schedule another frame so the compositor learns about it.
  ScheduleComposite();
}

void RenderWidget::OnSwapBuffersPosted() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersPosted");
}

void RenderWidget::OnSwapBuffersComplete() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersComplete");

  // Notify subclasses that composited rendering was flushed to the screen.
  DidFlushPaint();
}

void RenderWidget::OnHandleInputEvent(const blink::WebInputEvent* input_event,
                                      const ui::LatencyInfo& latency_info) {
  if (!input_event)
    return;
  base::AutoReset<bool> handling_input_event_resetter(&handling_input_event_,
                                                      true);
  base::AutoReset<WebInputEvent::Type> handling_event_type_resetter(
      &handling_event_type_, input_event->type);

  // Calls into |didOverscroll()| while handling this event will populate
  // |event_overscroll|, which in turn will be bundled with the event ack.
  scoped_ptr<DidOverscrollParams> event_overscroll;
  base::AutoReset<scoped_ptr<DidOverscrollParams>*>
      handling_event_overscroll_resetter(&handling_event_overscroll_,
                                         &event_overscroll);

#if defined(OS_ANDROID)
  // On Android, when a key is pressed or sent from the Keyboard using IME,
  // |AdapterInputConnection| generates input key events to make sure all JS
  // listeners that monitor KeyUp and KeyDown events receive the proper key
  // code. Since this input key event comes from IME, we need to set the
  // IME event guard here to make sure it does not interfere with other IME
  // events.
  scoped_ptr<ImeEventGuard> ime_event_guard_maybe;
  if (WebInputEvent::isKeyboardEventType(input_event->type)) {
    const WebKeyboardEvent& key_event =
        *static_cast<const WebKeyboardEvent*>(input_event);
    // Some keys are special and it's essential that no events get blocked.
    if (key_event.nativeKeyCode != AKEYCODE_TAB &&
        key_event.nativeKeyCode != AKEYCODE_DPAD_CENTER &&
        key_event.nativeKeyCode != AKEYCODE_DPAD_LEFT &&
        key_event.nativeKeyCode != AKEYCODE_DPAD_RIGHT &&
        key_event.nativeKeyCode != AKEYCODE_DPAD_UP &&
        key_event.nativeKeyCode != AKEYCODE_DPAD_DOWN)
      ime_event_guard_maybe.reset(new ImeEventGuard(this));
  }
#endif

  base::TimeTicks start_time;
  if (base::TimeTicks::IsHighResolution())
    start_time = base::TimeTicks::Now();

  TRACE_EVENT1("renderer,benchmark", "RenderWidget::OnHandleInputEvent",
               "event", WebInputEventTraits::GetName(input_event->type));
  TRACE_EVENT_SYNTHETIC_DELAY_BEGIN("blink.HandleInputEvent");
  TRACE_EVENT_WITH_FLOW1("input,benchmark",
                         "LatencyInfo.Flow",
                         TRACE_ID_DONT_MANGLE(latency_info.trace_id()),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "HandleInputEventMain");

  // If we don't have a high res timer, these metrics won't be accurate enough
  // to be worth collecting. Note that this does introduce some sampling bias.
  if (!start_time.is_null())
    LogInputEventLatencyUma(*input_event, start_time, latency_info);

  scoped_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor;
  ui::LatencyInfo swap_latency_info(latency_info);
  if (compositor_) {
    latency_info_swap_promise_monitor =
        compositor_->CreateLatencyInfoSwapPromiseMonitor(&swap_latency_info)
            .Pass();
  }

  bool prevent_default = false;
  if (WebInputEvent::isMouseEventType(input_event->type)) {
    const WebMouseEvent& mouse_event =
        *static_cast<const WebMouseEvent*>(input_event);
    TRACE_EVENT2("renderer", "HandleMouseMove",
                 "x", mouse_event.x, "y", mouse_event.y);
    context_menu_source_type_ = ui::MENU_SOURCE_MOUSE;
    prevent_default = WillHandleMouseEvent(mouse_event);
  }

  if (WebInputEvent::isKeyboardEventType(input_event->type)) {
    context_menu_source_type_ = ui::MENU_SOURCE_KEYBOARD;
#if defined(OS_ANDROID)
    // The DPAD_CENTER key on Android has a dual semantic: (1) in the general
    // case it should behave like a select key (i.e. causing a click if a button
    // is focused). However, if a text field is focused (2), its intended
    // behavior is to just show the IME and don't propagate the key.
    // A typical use case is a web form: the DPAD_CENTER should bring up the IME
    // when clicked on an input text field and cause the form submit if clicked
    // when the submit button is focused, but not vice-versa.
    // The UI layer takes care of translating DPAD_CENTER into a RETURN key,
    // but at this point we have to swallow the event for the scenario (2).
    const WebKeyboardEvent& key_event =
        *static_cast<const WebKeyboardEvent*>(input_event);
    if (key_event.nativeKeyCode == AKEYCODE_DPAD_CENTER &&
        GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE) {
      OnShowImeIfNeeded();
      prevent_default = true;
    }
#endif
  }

  if (WebInputEvent::isGestureEventType(input_event->type)) {
    const WebGestureEvent& gesture_event =
        *static_cast<const WebGestureEvent*>(input_event);
    context_menu_source_type_ = ui::MENU_SOURCE_TOUCH;
    prevent_default = prevent_default || WillHandleGestureEvent(gesture_event);
  }

  bool processed = prevent_default;
  if (input_event->type != WebInputEvent::Char || !suppress_next_char_events_) {
    suppress_next_char_events_ = false;
    if (!processed && webwidget_)
      processed = webwidget_->handleInputEvent(*input_event);
  }

  // If this RawKeyDown event corresponds to a browser keyboard shortcut and
  // it's not processed by webkit, then we need to suppress the upcoming Char
  // events.
  bool is_keyboard_shortcut =
      input_event->type == WebInputEvent::RawKeyDown &&
      static_cast<const WebKeyboardEvent*>(input_event)->isBrowserShortcut;
  if (!processed && is_keyboard_shortcut)
    suppress_next_char_events_ = true;

  InputEventAckState ack_result = processed ?
      INPUT_EVENT_ACK_STATE_CONSUMED : INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  if (!processed &&  input_event->type == WebInputEvent::TouchStart) {
    const WebTouchEvent& touch_event =
        *static_cast<const WebTouchEvent*>(input_event);
    // Hit-test for all the pressed touch points. If there is a touch-handler
    // for any of the touch points, then the renderer should continue to receive
    // touch events.
    ack_result = INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
    for (size_t i = 0; i < touch_event.touchesLength; ++i) {
      if (touch_event.touches[i].state == WebTouchPoint::StatePressed &&
          HasTouchEventHandlersAt(
              gfx::ToFlooredPoint(touch_event.touches[i].position))) {
        ack_result = INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
        break;
      }
    }
  }

  // Send mouse wheel events and their disposition to the compositor thread, so
  // that they can be used to produce the elastic overscroll effect on Mac.
  if (input_event->type == WebInputEvent::MouseWheel) {
    ObserveWheelEventAndResult(
        static_cast<const WebMouseWheelEvent&>(*input_event),
        event_overscroll ? event_overscroll->latest_overscroll_delta
                         : gfx::Vector2dF(),
        processed);
  }

  bool frame_pending = compositor_ && compositor_->BeginMainFrameRequested();

  // If we don't have a fast and accurate Now(), we assume the input handlers
  // are heavy and rate limit them.
  bool rate_limiting_wanted =
      input_event->type == WebInputEvent::MouseMove ||
      input_event->type == WebInputEvent::MouseWheel;
  if (rate_limiting_wanted && !start_time.is_null()) {
      base::TimeTicks end_time = base::TimeTicks::Now();
      total_input_handling_time_this_frame_ += (end_time - start_time);
      rate_limiting_wanted =
          total_input_handling_time_this_frame_.InMicroseconds() >
          kInputHandlingTimeThrottlingThresholdMicroseconds;
  }

  TRACE_EVENT_SYNTHETIC_DELAY_END("blink.HandleInputEvent");

  // Note that we can't use handling_event_type_ here since it will be overriden
  // by reentrant calls for events after the paused one.
  bool no_ack = ignore_ack_for_mouse_move_from_debugger_ &&
      input_event->type == WebInputEvent::MouseMove;
  if (WebInputEventTraits::WillReceiveAckFromRenderer(*input_event) &&
      !no_ack) {
    InputEventAck ack(input_event->type, ack_result, swap_latency_info,
                      event_overscroll.Pass(),
                      WebInputEventTraits::GetUniqueTouchEventId(*input_event));
    scoped_ptr<IPC::Message> response(
        new InputHostMsg_HandleInputEvent_ACK(routing_id_, ack));
    if (rate_limiting_wanted && frame_pending && !is_hidden_) {
      // We want to rate limit the input events in this case, so we'll wait for
      // painting to finish before ACKing this message.
      TRACE_EVENT_INSTANT0("renderer",
        "RenderWidget::OnHandleInputEvent ack throttled",
        TRACE_EVENT_SCOPE_THREAD);
      if (pending_input_event_ack_) {
        TRACE_EVENT_ASYNC_END0("input", "RenderWidget::ThrottledInputEventAck",
                               pending_input_event_ack_.get());
        // As two different kinds of events could cause us to postpone an ack
        // we send it now, if we have one pending. The Browser should never
        // send us the same kind of event we are delaying the ack for.
        Send(pending_input_event_ack_.release());
      }
      pending_input_event_ack_ = response.Pass();
      TRACE_EVENT_ASYNC_BEGIN0("input", "RenderWidget::ThrottledInputEventAck",
                               pending_input_event_ack_.get());
      if (compositor_)
        compositor_->NotifyInputThrottledUntilCommit();
    } else {
      Send(response.release());
    }
  } else {
    DCHECK(!event_overscroll) << "Unexpected overscroll for un-acked event";
  }
  if (!no_ack && RenderThreadImpl::current()) {
    RenderThreadImpl::current()
        ->GetRendererScheduler()
        ->DidHandleInputEventOnMainThread(*input_event);
  }
  if (input_event->type == WebInputEvent::MouseMove)
    ignore_ack_for_mouse_move_from_debugger_ = false;

#if defined(OS_ANDROID)
  // Allow the IME to be shown when the focus changes as a consequence
  // of a processed touch end event.
  if (input_event->type == WebInputEvent::TouchEnd && processed)
    UpdateTextInputState(SHOW_IME_IF_NEEDED, FROM_NON_IME);
#elif defined(USE_AURA)
  // Show the virtual keyboard if enabled and a user gesture triggers a focus
  // change.
  if (processed && (input_event->type == WebInputEvent::TouchEnd ||
                    input_event->type == WebInputEvent::MouseUp)) {
    UpdateTextInputState(SHOW_IME_IF_NEEDED, FROM_IME);
  }
#endif

  if (!prevent_default) {
    if (WebInputEvent::isKeyboardEventType(input_event->type))
      DidHandleKeyEvent();
  }

// TODO(rouslan): Fix ChromeOS and Windows 8 behavior of autofill popup with
// virtual keyboard.
#if !defined(OS_ANDROID)
  // Virtual keyboard is not supported, so react to focus change immediately.
  if (processed && (input_event->type == WebInputEvent::TouchEnd ||
                    input_event->type == WebInputEvent::MouseUp)) {
    FocusChangeComplete();
  }
#endif
}

void RenderWidget::OnCursorVisibilityChange(bool is_visible) {
  if (webwidget_)
    webwidget_->setCursorVisibilityState(is_visible);
}

void RenderWidget::OnMouseCaptureLost() {
  if (webwidget_)
    webwidget_->mouseCaptureLost();
}

void RenderWidget::OnSetFocus(bool enable) {
  if (webwidget_)
    webwidget_->setFocus(enable);
}

void RenderWidget::FlushPendingInputEventAck() {
  if (pending_input_event_ack_) {
    TRACE_EVENT_ASYNC_END0("input", "RenderWidget::ThrottledInputEventAck",
                           pending_input_event_ack_.get());
    Send(pending_input_event_ack_.release());
  }
  total_input_handling_time_this_frame_ = base::TimeDelta();
}

///////////////////////////////////////////////////////////////////////////////
// WebWidgetClient

void RenderWidget::didAutoResize(const WebSize& new_size) {
  if (size_.width() != new_size.width || size_.height() != new_size.height) {
    size_ = new_size;

    if (resizing_mode_selector_->is_synchronous_mode()) {
      WebRect new_pos(rootWindowRect().x,
                      rootWindowRect().y,
                      new_size.width,
                      new_size.height);
      view_screen_rect_ = new_pos;
      window_screen_rect_ = new_pos;
    }

    AutoResizeCompositor();

    if (!resizing_mode_selector_->is_synchronous_mode())
      need_update_rect_for_auto_resize_ = true;
  }
}

void RenderWidget::AutoResizeCompositor()  {
  physical_backing_size_ = gfx::ScaleToCeiledSize(size_, device_scale_factor_);
  if (compositor_)
    compositor_->setViewportSize(size_, physical_backing_size_);
}

void RenderWidget::initializeLayerTreeView() {
  DCHECK(!host_closing_);

  compositor_ = RenderWidgetCompositor::Create(this, compositor_deps_);
  compositor_->setViewportSize(size_, physical_backing_size_);

  // For background pages and certain tests, we don't want to trigger
  // OutputSurface creation.
  if (compositor_never_visible_ || !RenderThreadImpl::current())
    compositor_->SetNeverVisible();

  StartCompositor();
}

void RenderWidget::WillCloseLayerTreeView() {
  if (host_closing_)
    return;

  // Prevent new compositors or output surfaces from being created.
  host_closing_ = true;

  // Always send this notification to prevent new layer tree views from
  // being created, even if one hasn't been created yet.
  if (webwidget_)
    webwidget_->willCloseLayerTreeView();
}

blink::WebLayerTreeView* RenderWidget::layerTreeView() {
  return compositor_.get();
}

void RenderWidget::didMeaningfulLayout(blink::WebMeaningfulLayout layout_type) {
  if (layout_type == blink::WebMeaningfulLayout::VisuallyNonEmpty) {
    QueueMessage(new ViewHostMsg_DidFirstVisuallyNonEmptyPaint(routing_id_),
                 MESSAGE_DELIVERY_POLICY_WITH_VISUAL_STATE);
  }

  FOR_EACH_OBSERVER(RenderFrameImpl, render_frames_,
                    DidMeaningfulLayout(layout_type));
}

void RenderWidget::WillBeginCompositorFrame() {
  TRACE_EVENT0("gpu", "RenderWidget::willBeginCompositorFrame");

  // The UpdateTextInputState can result in further layout and possibly
  // enable GPU acceleration so they need to be called before any painting
  // is done.
  UpdateTextInputState(NO_SHOW_IME, FROM_NON_IME);
  UpdateSelectionBounds();
}

void RenderWidget::DidCommitCompositorFrame() {
  FOR_EACH_OBSERVER(RenderFrameImpl, render_frames_,
                    DidCommitCompositorFrame());
  FOR_EACH_OBSERVER(RenderFrameProxy, render_frame_proxies_,
                    DidCommitCompositorFrame());
#if defined(VIDEO_HOLE)
  FOR_EACH_OBSERVER(RenderFrameImpl, video_hole_frames_,
                    DidCommitCompositorFrame());
#endif  // defined(VIDEO_HOLE)
  FlushPendingInputEventAck();
}

void RenderWidget::DidCommitAndDrawCompositorFrame() {
  // NOTE: Tests may break if this event is renamed or moved. See
  // tab_capture_performancetest.cc.
  TRACE_EVENT0("gpu", "RenderWidget::DidCommitAndDrawCompositorFrame");
  // Notify subclasses that we initiated the paint operation.
  DidInitiatePaint();
}

void RenderWidget::DidCompleteSwapBuffers() {
  TRACE_EVENT0("renderer", "RenderWidget::DidCompleteSwapBuffers");

  // Notify subclasses threaded composited rendering was flushed to the screen.
  DidFlushPaint();

  if (!next_paint_flags_ &&
      !need_update_rect_for_auto_resize_ &&
      !plugin_window_moves_.size()) {
    return;
  }

  ViewHostMsg_UpdateRect_Params params;
  params.view_size = size_;
  params.plugin_window_moves.swap(plugin_window_moves_);
  params.flags = next_paint_flags_;

  Send(new ViewHostMsg_UpdateRect(routing_id_, params));
  next_paint_flags_ = 0;
  need_update_rect_for_auto_resize_ = false;
}

void RenderWidget::ScheduleComposite() {
  if (compositor_ &&
      compositor_deps_->GetCompositorImplThreadTaskRunner().get()) {
    compositor_->setNeedsAnimate();
  }
}

void RenderWidget::ScheduleCompositeWithForcedRedraw() {
  if (compositor_) {
    // Regardless of whether threaded compositing is enabled, always
    // use this mechanism to force the compositor to redraw. However,
    // the invalidation code path below is still needed for the
    // non-threaded case.
    compositor_->SetNeedsForcedRedraw();
  }
  ScheduleComposite();
}

// static
scoped_ptr<cc::SwapPromise> RenderWidget::QueueMessageImpl(
    IPC::Message* msg,
    MessageDeliveryPolicy policy,
    FrameSwapMessageQueue* frame_swap_message_queue,
    scoped_refptr<IPC::SyncMessageFilter> sync_message_filter,
    int source_frame_number) {
  bool first_message_for_frame = false;
  frame_swap_message_queue->QueueMessageForFrame(policy,
                                                 source_frame_number,
                                                 make_scoped_ptr(msg),
                                                 &first_message_for_frame);
  if (first_message_for_frame) {
    scoped_ptr<cc::SwapPromise> promise(new QueueMessageSwapPromise(
        sync_message_filter, frame_swap_message_queue, source_frame_number));
    return promise;
  }
  return nullptr;
}

void RenderWidget::QueueMessage(IPC::Message* msg,
                                MessageDeliveryPolicy policy) {
  // RenderThreadImpl::current() is NULL in some tests.
  if (!compositor_ || !RenderThreadImpl::current()) {
    Send(msg);
    return;
  }

  scoped_ptr<cc::SwapPromise> swap_promise =
      QueueMessageImpl(msg,
                       policy,
                       frame_swap_message_queue_.get(),
                       RenderThreadImpl::current()->sync_message_filter(),
                       compositor_->GetSourceFrameNumber());

  if (swap_promise) {
    compositor_->QueueSwapPromise(swap_promise.Pass());
    // Request a commit. This might either A) request a commit ahead of time
    // or B) request a commit which is not needed because there are not
    // pending updates. If B) then the commit will be skipped and the swap
    // promises will be broken (see EarlyOut_NoUpdates). To achieve that we
    // call SetNeedsUpdateLayers instead of SetNeedsCommit so that
    // can_cancel_commit is not unset.
    compositor_->SetNeedsUpdateLayers();
  }
}

void RenderWidget::didChangeCursor(const WebCursorInfo& cursor_info) {
  // TODO(darin): Eliminate this temporary.
  WebCursor cursor;
  InitializeCursorFromWebCursorInfo(&cursor, cursor_info);
  // Only send a SetCursor message if we need to make a change.
  if (!current_cursor_.IsEqual(cursor)) {
    current_cursor_ = cursor;
    Send(new ViewHostMsg_SetCursor(routing_id_, cursor));
  }
}

// We are supposed to get a single call to Show for a newly created RenderWidget
// that was created via RenderWidget::CreateWebView.  So, we wait until this
// point to dispatch the ShowWidget message.
//
// This method provides us with the information about how to display the newly
// created RenderWidget (i.e., as a blocked popup or as a new tab).
//
void RenderWidget::show(WebNavigationPolicy) {
  DCHECK(!did_show_) << "received extraneous Show call";
  DCHECK(routing_id_ != MSG_ROUTING_NONE);
  DCHECK(opener_id_ != MSG_ROUTING_NONE);

  if (did_show_)
    return;

  did_show_ = true;
  // NOTE: initial_rect_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if as_popup is false, or the browser
  // process will impose a default position otherwise.
  Send(new ViewHostMsg_ShowWidget(opener_id_, routing_id_, initial_rect_));
  SetPendingWindowRect(initial_rect_);
}

void RenderWidget::didFocus() {
}

void RenderWidget::DoDeferredClose() {
  WillCloseLayerTreeView();
  Send(new ViewHostMsg_Close(routing_id_));
}

void RenderWidget::NotifyOnClose() {
  FOR_EACH_OBSERVER(RenderFrameImpl, render_frames_, WidgetWillClose());
}

void RenderWidget::closeWidgetSoon() {
  DCHECK(content::RenderThread::Get());
  if (is_swapped_out_) {
    // This widget is currently swapped out, and the active widget is in a
    // different process.  Have the browser route the close request to the
    // active widget instead, so that the correct unload handlers are run.
    Send(new ViewHostMsg_RouteCloseEvent(routing_id_));
    return;
  }

  // If a page calls window.close() twice, we'll end up here twice, but that's
  // OK.  It is safe to send multiple Close messages.

  // Ask the RenderWidgetHost to initiate close.  We could be called from deep
  // in Javascript.  If we ask the RendwerWidgetHost to close now, the window
  // could be closed before the JS finishes executing.  So instead, post a
  // message back to the message loop, which won't run until the JS is
  // complete, and then the Close message can be sent.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&RenderWidget::DoDeferredClose, this));
}

void RenderWidget::QueueSyntheticGesture(
    scoped_ptr<SyntheticGestureParams> gesture_params,
    const SyntheticGestureCompletionCallback& callback) {
  DCHECK(!callback.is_null());

  pending_synthetic_gesture_callbacks_.push(callback);

  SyntheticGesturePacket gesture_packet;
  gesture_packet.set_gesture_params(gesture_params.Pass());

  Send(new InputHostMsg_QueueSyntheticGesture(routing_id_, gesture_packet));
}

void RenderWidget::Close() {
  screen_metrics_emulator_.reset();
  WillCloseLayerTreeView();
  compositor_.reset();
  if (webwidget_) {
    webwidget_->close();
    webwidget_ = NULL;
  }
}

WebRect RenderWidget::windowRect() {
  if (pending_window_rect_count_)
    return pending_window_rect_;

  return view_screen_rect_;
}

void RenderWidget::setToolTipText(const blink::WebString& text,
                                  WebTextDirection hint) {
  Send(new ViewHostMsg_SetTooltipText(routing_id_, text, hint));
}

void RenderWidget::setWindowRect(const WebRect& rect) {
  WebRect window_rect = rect;
  if (popup_origin_scale_for_emulation_) {
    float scale = popup_origin_scale_for_emulation_;
    window_rect.x = popup_screen_origin_for_emulation_.x() +
        (window_rect.x - popup_view_origin_for_emulation_.x()) * scale;
    window_rect.y = popup_screen_origin_for_emulation_.y() +
        (window_rect.y - popup_view_origin_for_emulation_.y()) * scale;
  }

  if (!resizing_mode_selector_->is_synchronous_mode()) {
    if (did_show_) {
      Send(new ViewHostMsg_RequestMove(routing_id_, window_rect));
      SetPendingWindowRect(window_rect);
    } else {
      initial_rect_ = window_rect;
    }
  } else {
    SetWindowRectSynchronously(window_rect);
  }
}

void RenderWidget::SetPendingWindowRect(const WebRect& rect) {
  pending_window_rect_ = rect;
  pending_window_rect_count_++;
}

WebRect RenderWidget::rootWindowRect() {
  if (pending_window_rect_count_) {
    // NOTE(mbelshe): If there is a pending_window_rect_, then getting
    // the RootWindowRect is probably going to return wrong results since the
    // browser may not have processed the Move yet.  There isn't really anything
    // good to do in this case, and it shouldn't happen - since this size is
    // only really needed for windowToScreen, which is only used for Popups.
    return pending_window_rect_;
  }

  return window_screen_rect_;
}

WebRect RenderWidget::windowResizerRect() {
  return resizer_rect_;
}

void RenderWidget::OnImeSetComposition(
    const base::string16& text,
    const std::vector<WebCompositionUnderline>& underlines,
    int selection_start, int selection_end) {
  if (!ShouldHandleImeEvent())
    return;
  ImeEventGuard guard(this);
  if (!webwidget_->setComposition(
      text, WebVector<WebCompositionUnderline>(underlines),
      selection_start, selection_end)) {
    // If we failed to set the composition text, then we need to let the browser
    // process to cancel the input method's ongoing composition session, to make
    // sure we are in a consistent state.
    Send(new InputHostMsg_ImeCancelComposition(routing_id()));
  }
  UpdateCompositionInfo(true);
}

void RenderWidget::OnImeConfirmComposition(const base::string16& text,
                                           const gfx::Range& replacement_range,
                                           bool keep_selection) {
  if (!ShouldHandleImeEvent())
    return;
  ImeEventGuard guard(this);
  handling_input_event_ = true;
  if (text.length())
    webwidget_->confirmComposition(text);
  else if (keep_selection)
    webwidget_->confirmComposition(WebWidget::KeepSelection);
  else
    webwidget_->confirmComposition(WebWidget::DoNotKeepSelection);
  handling_input_event_ = false;
  UpdateCompositionInfo(true);
}

void RenderWidget::OnRepaint(gfx::Size size_to_paint) {
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  // Even if the browser provides an empty damage rect, it's still expecting to
  // receive a repaint ack so just damage the entire widget bounds.
  if (size_to_paint.IsEmpty()) {
    size_to_paint = size_;
  }

  set_next_paint_is_repaint_ack();
  if (compositor_)
    compositor_->SetNeedsRedrawRect(gfx::Rect(size_to_paint));
}

void RenderWidget::OnSyntheticGestureCompleted() {
  DCHECK(!pending_synthetic_gesture_callbacks_.empty());

  pending_synthetic_gesture_callbacks_.front().Run();
  pending_synthetic_gesture_callbacks_.pop();
}

void RenderWidget::OnSetTextDirection(WebTextDirection direction) {
  if (!webwidget_)
    return;
  webwidget_->setTextDirection(direction);
}

void RenderWidget::OnUpdateScreenRects(const gfx::Rect& view_screen_rect,
                                       const gfx::Rect& window_screen_rect) {
  if (screen_metrics_emulator_) {
    screen_metrics_emulator_->OnUpdateScreenRectsMessage(
        view_screen_rect, window_screen_rect);
  } else {
    view_screen_rect_ = view_screen_rect;
    window_screen_rect_ = window_screen_rect;
  }
  Send(new ViewHostMsg_UpdateScreenRects_ACK(routing_id()));
}

void RenderWidget::OnSetSurfaceIdNamespace(uint32_t surface_id_namespace) {
  if (compositor_)
    compositor_->SetSurfaceIdNamespace(surface_id_namespace);
}

void RenderWidget::showImeIfNeeded() {
  OnShowImeIfNeeded();
}

void RenderWidget::OnShowImeIfNeeded() {
#if defined(OS_ANDROID) || defined(USE_AURA)
  UpdateTextInputState(SHOW_IME_IF_NEEDED, FROM_NON_IME);
#endif

// TODO(rouslan): Fix ChromeOS and Windows 8 behavior of autofill popup with
// virtual keyboard.
#if !defined(OS_ANDROID)
  FocusChangeComplete();
#endif
}

#if defined(OS_ANDROID)
void RenderWidget::OnImeEventSentForAck(const blink::WebTextInputInfo& info) {
  text_input_info_history_.push_back(info);
}

void RenderWidget::OnImeEventAck() {
  DCHECK_GE(text_input_info_history_.size(), 1u);
  text_input_info_history_.pop_front();
}
#endif

bool RenderWidget::ShouldHandleImeEvent() {
#if defined(OS_ANDROID)
  if (!webwidget_)
    return false;

  // We cannot handle IME events if there is any chance that the event we are
  // receiving here from the browser is based on the state that is different
  // from our current one as indicated by |text_input_info_|.
  // The states the browser might be in are:
  // text_input_info_history_[0] - current state ack'd by browser
  // text_input_info_history_[1...N] - pending state changes
  for (size_t i = 0u; i < text_input_info_history_.size() - 1u; ++i) {
    if (text_input_info_history_[i] != text_input_info_)
      return false;
  }
  return true;
#else
  return !!webwidget_;
#endif
}

bool RenderWidget::SendAckForMouseMoveFromDebugger() {
  if (handling_event_type_ == WebInputEvent::MouseMove) {
    // If we pause multiple times during a single mouse move event, we should
    // only send ACK once.
    if (!ignore_ack_for_mouse_move_from_debugger_) {
      InputEventAck ack(handling_event_type_, INPUT_EVENT_ACK_STATE_CONSUMED);
      Send(new InputHostMsg_HandleInputEvent_ACK(routing_id_, ack));
      return true;
    }
  }
  return false;
}

void RenderWidget::IgnoreAckForMouseMoveFromDebugger() {
  ignore_ack_for_mouse_move_from_debugger_ = true;
}

void RenderWidget::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor_ == device_scale_factor)
    return;

  device_scale_factor_ = device_scale_factor;
  ScheduleComposite();
}

bool RenderWidget::SetDeviceColorProfile(
    const std::vector<char>& color_profile) {
  if (device_color_profile_ == color_profile)
    return false;

  device_color_profile_ = color_profile;
  return true;
}

void RenderWidget::ResetDeviceColorProfileForTesting() {
  if (!device_color_profile_.empty())
    device_color_profile_.clear();
  device_color_profile_.push_back('0');
}

void RenderWidget::OnOrientationChange() {
}

gfx::Vector2d RenderWidget::GetScrollOffset() {
  // Bare RenderWidgets don't support scroll offset.
  return gfx::Vector2d();
}

void RenderWidget::SetHidden(bool hidden) {
  if (is_hidden_ == hidden)
    return;

  // The status has changed.  Tell the RenderThread about it and ensure
  // throttled acks are released in case frame production ceases.
  is_hidden_ = hidden;
  FlushPendingInputEventAck();

  if (is_hidden_)
    RenderThreadImpl::current()->WidgetHidden();
  else
    RenderThreadImpl::current()->WidgetRestored();

  if (render_widget_scheduling_state_)
    render_widget_scheduling_state_->SetHidden(hidden);
}

void RenderWidget::DidToggleFullscreen() {
  if (!webwidget_)
    return;

  if (is_fullscreen_granted_) {
    webwidget_->didEnterFullScreen();
  } else {
    webwidget_->didExitFullScreen();
  }
}

bool RenderWidget::next_paint_is_resize_ack() const {
  return ViewHostMsg_UpdateRect_Flags::is_resize_ack(next_paint_flags_);
}

void RenderWidget::set_next_paint_is_resize_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK;
}

void RenderWidget::set_next_paint_is_repaint_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_REPAINT_ACK;
}

static bool IsDateTimeInput(ui::TextInputType type) {
  return type == ui::TEXT_INPUT_TYPE_DATE ||
      type == ui::TEXT_INPUT_TYPE_DATE_TIME ||
      type == ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL ||
      type == ui::TEXT_INPUT_TYPE_MONTH ||
      type == ui::TEXT_INPUT_TYPE_TIME ||
      type == ui::TEXT_INPUT_TYPE_WEEK;
}


void RenderWidget::StartHandlingImeEvent() {
  DCHECK(!handling_ime_event_);
  handling_ime_event_ = true;
}

void RenderWidget::FinishHandlingImeEvent() {
  DCHECK(handling_ime_event_);
  handling_ime_event_ = false;
  // While handling an ime event, text input state and selection bounds updates
  // are ignored. These must explicitly be updated once finished handling the
  // ime event.
  UpdateSelectionBounds();
#if defined(OS_ANDROID)
  UpdateTextInputState(NO_SHOW_IME, FROM_IME);
#endif
}

void RenderWidget::UpdateTextInputState(ShowIme show_ime,
                                        ChangeSource change_source) {
  TRACE_EVENT0("renderer", "RenderWidget::UpdateTextInputState");
  if (handling_ime_event_)
    return;
  ui::TextInputType new_type = GetTextInputType();
  if (IsDateTimeInput(new_type))
    return;  // Not considered as a text input field in WebKit/Chromium.

  blink::WebTextInputInfo new_info;
  if (webwidget_)
    new_info = webwidget_->textInputInfo();
  const ui::TextInputMode new_mode = ConvertInputMode(new_info.inputMode);

  bool new_can_compose_inline = CanComposeInline();

  // Only sends text input params if they are changed or if the ime should be
  // shown.
  if (show_ime == SHOW_IME_IF_NEEDED ||
      (text_input_type_ != new_type ||
       text_input_mode_ != new_mode ||
       text_input_info_ != new_info ||
       can_compose_inline_ != new_can_compose_inline)
#if defined(OS_ANDROID)
      || text_field_is_dirty_
#endif
      ) {
    ViewHostMsg_TextInputState_Params params;
    params.type = new_type;
    params.mode = new_mode;
    params.flags = new_info.flags;
    params.value = new_info.value.utf8();
    params.selection_start = new_info.selectionStart;
    params.selection_end = new_info.selectionEnd;
    params.composition_start = new_info.compositionStart;
    params.composition_end = new_info.compositionEnd;
    params.can_compose_inline = new_can_compose_inline;
    params.show_ime_if_needed = (show_ime == SHOW_IME_IF_NEEDED);
#if defined(USE_AURA)
    params.is_non_ime_change = true;
#endif
#if defined(OS_ANDROID)
    params.is_non_ime_change = (change_source == FROM_NON_IME) ||
                         text_field_is_dirty_;
    if (params.is_non_ime_change)
      OnImeEventSentForAck(new_info);
    text_field_is_dirty_ = false;
#endif
    Send(new ViewHostMsg_TextInputStateChanged(routing_id(), params));

    text_input_info_ = new_info;
    text_input_type_ = new_type;
    text_input_mode_ = new_mode;
    can_compose_inline_ = new_can_compose_inline;
    text_input_flags_ = new_info.flags;
  }
}

void RenderWidget::GetSelectionBounds(gfx::Rect* focus, gfx::Rect* anchor) {
  WebRect focus_webrect;
  WebRect anchor_webrect;
  webwidget_->selectionBounds(focus_webrect, anchor_webrect);
  *focus = focus_webrect;
  *anchor = anchor_webrect;
}

void RenderWidget::UpdateSelectionBounds() {
  TRACE_EVENT0("renderer", "RenderWidget::UpdateSelectionBounds");
  if (!webwidget_)
    return;
  if (handling_ime_event_)
    return;

#if defined(USE_AURA)
  // TODO(mohsen): For now, always send explicit selection IPC notifications for
  // Aura beucause composited selection updates are not working for webview tags
  // which regresses IME inside webview. Remove this when composited selection
  // updates are fixed for webviews. See, http://crbug.com/510568.
  bool send_ipc = true;
#else
  // With composited selection updates, the selection bounds will be reported
  // directly by the compositor, in which case explicit IPC selection
  // notifications should be suppressed.
  bool send_ipc =
      !blink::WebRuntimeFeatures::isCompositedSelectionUpdateEnabled();
#endif
  if (send_ipc) {
    ViewHostMsg_SelectionBounds_Params params;
    GetSelectionBounds(&params.anchor_rect, &params.focus_rect);
    if (selection_anchor_rect_ != params.anchor_rect ||
        selection_focus_rect_ != params.focus_rect) {
      selection_anchor_rect_ = params.anchor_rect;
      selection_focus_rect_ = params.focus_rect;
      webwidget_->selectionTextDirection(params.focus_dir, params.anchor_dir);
      params.is_anchor_first = webwidget_->isSelectionAnchorFirst();
      Send(new ViewHostMsg_SelectionBoundsChanged(routing_id_, params));
    }
  }

  UpdateCompositionInfo(false);
}

// Check blink::WebTextInputType and ui::TextInputType is kept in sync.
#define STATIC_ASSERT_WTIT_ENUM_MATCH(a, b)            \
    static_assert(int(blink::WebTextInputType##a)      \
                      == int(ui::TEXT_INPUT_TYPE_##b), \
                  "mismatching enums: " #a)

STATIC_ASSERT_WTIT_ENUM_MATCH(None,            NONE);
STATIC_ASSERT_WTIT_ENUM_MATCH(Text,            TEXT);
STATIC_ASSERT_WTIT_ENUM_MATCH(Password,        PASSWORD);
STATIC_ASSERT_WTIT_ENUM_MATCH(Search,          SEARCH);
STATIC_ASSERT_WTIT_ENUM_MATCH(Email,           EMAIL);
STATIC_ASSERT_WTIT_ENUM_MATCH(Number,          NUMBER);
STATIC_ASSERT_WTIT_ENUM_MATCH(Telephone,       TELEPHONE);
STATIC_ASSERT_WTIT_ENUM_MATCH(URL,             URL);
STATIC_ASSERT_WTIT_ENUM_MATCH(Date,            DATE);
STATIC_ASSERT_WTIT_ENUM_MATCH(DateTime,        DATE_TIME);
STATIC_ASSERT_WTIT_ENUM_MATCH(DateTimeLocal,   DATE_TIME_LOCAL);
STATIC_ASSERT_WTIT_ENUM_MATCH(Month,           MONTH);
STATIC_ASSERT_WTIT_ENUM_MATCH(Time,            TIME);
STATIC_ASSERT_WTIT_ENUM_MATCH(Week,            WEEK);
STATIC_ASSERT_WTIT_ENUM_MATCH(TextArea,        TEXT_AREA);
STATIC_ASSERT_WTIT_ENUM_MATCH(ContentEditable, CONTENT_EDITABLE);
STATIC_ASSERT_WTIT_ENUM_MATCH(DateTimeField,   DATE_TIME_FIELD);

ui::TextInputType RenderWidget::WebKitToUiTextInputType(
    blink::WebTextInputType type) {
  // Check the type is in the range representable by ui::TextInputType.
  DCHECK_LE(type, static_cast<int>(ui::TEXT_INPUT_TYPE_MAX)) <<
    "blink::WebTextInputType and ui::TextInputType not synchronized";
  return static_cast<ui::TextInputType>(type);
}

ui::TextInputType RenderWidget::GetTextInputType() {
  if (webwidget_)
    return WebKitToUiTextInputType(webwidget_->textInputType());
  return ui::TEXT_INPUT_TYPE_NONE;
}

void RenderWidget::UpdateCompositionInfo(bool should_update_range) {
#if defined(OS_ANDROID)
  // TODO(yukawa): Start sending character bounds when the browser side
  // implementation becomes ready (crbug.com/424866).
#else
  TRACE_EVENT0("renderer", "RenderWidget::UpdateCompositionInfo");
  gfx::Range range = gfx::Range();
  if (should_update_range) {
    GetCompositionRange(&range);
  } else {
    range = composition_range_;
  }
  std::vector<gfx::Rect> character_bounds;
  GetCompositionCharacterBounds(&character_bounds);

  if (!ShouldUpdateCompositionInfo(range, character_bounds))
    return;
  composition_character_bounds_ = character_bounds;
  composition_range_ = range;
  Send(new InputHostMsg_ImeCompositionRangeChanged(
      routing_id(), composition_range_, composition_character_bounds_));
#endif
}

void RenderWidget::GetCompositionCharacterBounds(
    std::vector<gfx::Rect>* bounds) {
  DCHECK(bounds);
  bounds->clear();
}

void RenderWidget::GetCompositionRange(gfx::Range* range) {
  size_t location, length;
  if (webwidget_->compositionRange(&location, &length)) {
    range->set_start(location);
    range->set_end(location + length);
  } else if (webwidget_->caretOrSelectionRange(&location, &length)) {
    range->set_start(location);
    range->set_end(location + length);
  } else {
    *range = gfx::Range::InvalidRange();
  }
}

bool RenderWidget::ShouldUpdateCompositionInfo(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& bounds) {
  if (composition_range_ != range)
    return true;
  if (bounds.size() != composition_character_bounds_.size())
    return true;
  for (size_t i = 0; i < bounds.size(); ++i) {
    if (bounds[i] != composition_character_bounds_[i])
      return true;
  }
  return false;
}

#if defined(OS_ANDROID)
bool RenderWidget::DoesRecordFullLayer() const {
  return false;
}
#endif

bool RenderWidget::CanComposeInline() {
  return true;
}

WebScreenInfo RenderWidget::screenInfo() {
  return screen_info_;
}

float RenderWidget::deviceScaleFactor() {
  return device_scale_factor_;
}

void RenderWidget::resetInputMethod() {
  ImeEventGuard guard(this);
  // If the last text input type is not None, then we should finish any
  // ongoing composition regardless of the new text input type.
  if (text_input_type_ != ui::TEXT_INPUT_TYPE_NONE) {
    // If a composition text exists, then we need to let the browser process
    // to cancel the input method's ongoing composition session.
    if (webwidget_->confirmComposition())
      Send(new InputHostMsg_ImeCancelComposition(routing_id()));
  }

  UpdateCompositionInfo(true);
}

#if defined(OS_ANDROID)
void RenderWidget::showUnhandledTapUIIfNeeded(
    const WebPoint& tapped_position,
    const WebNode& tapped_node,
    bool page_changed) {
  DCHECK(handling_input_event_);
  bool should_trigger = !page_changed && tapped_node.isTextNode() &&
                        !tapped_node.isContentEditable() &&
                        !tapped_node.isInsideFocusableElementOrARIAWidget();
  if (should_trigger) {
    Send(new ViewHostMsg_ShowUnhandledTapUIIfNeeded(routing_id_,
        tapped_position.x, tapped_position.y));
  }
}
#endif

void RenderWidget::didHandleGestureEvent(
    const WebGestureEvent& event,
    bool event_cancelled) {
#if defined(OS_ANDROID) || defined(USE_AURA)
  if (event_cancelled)
    return;
  if (event.type == WebInputEvent::GestureTap) {
    UpdateTextInputState(SHOW_IME_IF_NEEDED, FROM_NON_IME);
  } else if (event.type == WebInputEvent::GestureLongPress) {
    DCHECK(webwidget_);
    if (webwidget_->textInputInfo().value.isEmpty())
      UpdateTextInputState(NO_SHOW_IME, FROM_NON_IME);
    else
      UpdateTextInputState(SHOW_IME_IF_NEEDED, FROM_NON_IME);
  }
#endif
}

void RenderWidget::didOverscroll(
    const blink::WebFloatSize& unusedDelta,
    const blink::WebFloatSize& accumulatedRootOverScroll,
    const blink::WebFloatPoint& position,
    const blink::WebFloatSize& velocity) {
  DidOverscrollParams params;
  params.accumulated_overscroll = gfx::Vector2dF(
      accumulatedRootOverScroll.width, accumulatedRootOverScroll.height);
  params.latest_overscroll_delta =
      gfx::Vector2dF(unusedDelta.width, unusedDelta.height);
  // TODO(sataya.m): don't negate velocity once http://crbug.com/499743 is
  // fixed.
  params.current_fling_velocity =
      gfx::Vector2dF(-velocity.width, -velocity.height);
  params.causal_event_viewport_point = gfx::PointF(position.x, position.y);

  // If we're currently handling an event, stash the overscroll data such that
  // it can be bundled in the event ack.
  if (handling_event_overscroll_) {
    handling_event_overscroll_->reset(new DidOverscrollParams(params));
    return;
  }

  Send(new InputHostMsg_DidOverscroll(routing_id_, params));
}

void RenderWidget::StartCompositor() {
  if (!is_hidden())
    compositor_->setVisible(true);
}

void RenderWidget::SchedulePluginMove(const WebPluginGeometry& move) {
  size_t i = 0;
  for (; i < plugin_window_moves_.size(); ++i) {
    if (plugin_window_moves_[i].window == move.window) {
      if (move.rects_valid) {
        plugin_window_moves_[i] = move;
      } else {
        plugin_window_moves_[i].visible = move.visible;
      }
      break;
    }
  }

  if (i == plugin_window_moves_.size())
    plugin_window_moves_.push_back(move);
}

void RenderWidget::CleanupWindowInPluginMoves(gfx::PluginWindowHandle window) {
  for (WebPluginGeometryVector::iterator i = plugin_window_moves_.begin();
       i != plugin_window_moves_.end(); ++i) {
    if (i->window == window) {
      plugin_window_moves_.erase(i);
      break;
    }
  }
}


RenderWidgetCompositor* RenderWidget::compositor() const {
  return compositor_.get();
}

bool RenderWidget::WillHandleMouseEvent(const blink::WebMouseEvent& event) {
  return false;
}

bool RenderWidget::WillHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  return false;
}

void RenderWidget::ObserveWheelEventAndResult(
    const blink::WebMouseWheelEvent& wheel_event,
    const gfx::Vector2dF& wheel_unused_delta,
    bool event_processed) {
  if (!compositor_deps_->IsElasticOverscrollEnabled())
    return;

  cc::InputHandlerScrollResult scroll_result;
  scroll_result.did_scroll = event_processed;
  scroll_result.did_overscroll_root = !wheel_unused_delta.IsZero();
  scroll_result.unused_scroll_delta = wheel_unused_delta;

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  InputHandlerManager* input_handler_manager =
      render_thread ? render_thread->input_handler_manager() : NULL;
  if (input_handler_manager) {
    input_handler_manager->ObserveWheelEventAndResultOnMainThread(
        routing_id_, wheel_event, scroll_result);
  }
}

void RenderWidget::hasTouchEventHandlers(bool has_handlers) {
  if (render_widget_scheduling_state_)
    render_widget_scheduling_state_->SetHasTouchHandler(has_handlers);
  Send(new ViewHostMsg_HasTouchEventHandlers(routing_id_, has_handlers));
}

// Check blink::WebTouchAction and  blink::WebTouchActionAuto is kept in sync
#define STATIC_ASSERT_WTI_ENUM_MATCH(a, b)                                         \
    static_assert(int(blink::WebTouchAction##a) == int(TOUCH_ACTION_##b), \
                  "mismatching enums: " #a)

inline content::TouchAction& operator|=(content::TouchAction& a,
                                        content::TouchAction b) {
  a = static_cast<content::TouchAction>(static_cast<int>(a) |
                                        static_cast<int>(b));
  return a;
}

void RenderWidget::setTouchAction(
    blink::WebTouchAction web_touch_action) {

  // Ignore setTouchAction calls that result from synthetic touch events (eg.
  // when blink is emulating touch with mouse).
  if (handling_event_type_ != WebInputEvent::TouchStart)
    return;

  // Verify the same values are used by the types so we can cast between them.
   STATIC_ASSERT_WTI_ENUM_MATCH(Auto,      AUTO);
   STATIC_ASSERT_WTI_ENUM_MATCH(None,      NONE);
   STATIC_ASSERT_WTI_ENUM_MATCH(PanLeft,   PAN_LEFT);
   STATIC_ASSERT_WTI_ENUM_MATCH(PanRight,  PAN_RIGHT);
   STATIC_ASSERT_WTI_ENUM_MATCH(PanX,      PAN_X);
   STATIC_ASSERT_WTI_ENUM_MATCH(PanUp,     PAN_UP);
   STATIC_ASSERT_WTI_ENUM_MATCH(PanDown,   PAN_DOWN);
   STATIC_ASSERT_WTI_ENUM_MATCH(PanY,      PAN_Y);
   STATIC_ASSERT_WTI_ENUM_MATCH(PinchZoom, PINCH_ZOOM);

   content::TouchAction content_touch_action =
       static_cast<content::TouchAction>(web_touch_action);
  Send(new InputHostMsg_SetTouchAction(routing_id_, content_touch_action));
}

void RenderWidget::didUpdateTextOfFocusedElementByNonUserInput() {
#if defined(OS_ANDROID)
  text_field_is_dirty_ = true;
#endif
}

bool RenderWidget::HasTouchEventHandlersAt(const gfx::Point& point) const {
  return true;
}

scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
RenderWidget::CreateGraphicsContext3D(bool compositor) {
  if (!webwidget_)
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuCompositing))
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  if (!RenderThreadImpl::current())
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  CauseForGpuLaunch cause =
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;
  scoped_refptr<GpuChannelHost> gpu_channel_host(
      RenderThreadImpl::current()->EstablishGpuChannelSync(cause));
  if (!gpu_channel_host.get())
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();

  // Explicitly disable antialiasing for the compositor. As of the time of
  // this writing, the only platform that supported antialiasing for the
  // compositor was Mac OS X, because the on-screen OpenGL context creation
  // code paths on Windows and Linux didn't yet have multisampling support.
  // Mac OS X essentially always behaves as though it's rendering offscreen.
  // Multisampling has a heavy cost especially on devices with relatively low
  // fill rate like most notebooks, and the Mac implementation would need to
  // be optimized to resolve directly into the IOSurface shared between the
  // GPU and browser processes. For these reasons and to avoid platform
  // disparities we explicitly disable antialiasing.
  blink::WebGraphicsContext3D::Attributes attributes;
  attributes.antialias = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;
  attributes.depth = false;
  attributes.stencil = false;
  bool lose_context_when_out_of_memory = true;
  WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits limits;
#if defined(OS_ANDROID)
  bool using_synchronous_compositing =
      SynchronousCompositorFactory::GetInstance();
  // If we raster too fast we become upload bound, and pending
  // uploads consume memory. For maximum upload throughput, we would
  // want to allow for upload_throughput * pipeline_time of pending
  // uploads, after which we are just wasting memory. Since we don't
  // know our upload throughput yet, this just caps our memory usage.
  // Synchronous compositor uses half because synchronous compositor
  // pipeline is only one frame deep. But twice of half for low end
  // because 16bit texture is not supported.
  size_t divider = using_synchronous_compositing ? 2 : 1;
  if (base::SysInfo::IsLowEndDevice())
    divider = 6;
  // For reference Nexus10 can upload 1MB in about 2.5ms.
  const double max_mb_uploaded_per_ms = 2.0 / (5 * divider);
  // Deadline to draw a frame to achieve 60 frames per second.
  const size_t kMillisecondsPerFrame = 16;
  // Assuming a two frame deep pipeline between the CPU and the GPU.
  size_t max_transfer_buffer_usage_mb =
      static_cast<size_t>(2 * kMillisecondsPerFrame * max_mb_uploaded_per_ms);
  static const size_t kBytesPerMegabyte = 1024 * 1024;
  // We keep the MappedMemoryReclaimLimit the same as the upload limit
  // to avoid unnecessarily stalling the compositor thread.
  limits.mapped_memory_reclaim_limit =
      max_transfer_buffer_usage_mb * kBytesPerMegabyte;
#endif
  if (compositor) {
    limits.command_buffer_size = 64 * 1024;
    limits.start_transfer_buffer_size = 64 * 1024;
    limits.min_transfer_buffer_size = 64 * 1024;
  }

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
      new WebGraphicsContext3DCommandBufferImpl(
          0, GetURLForGraphicsContext3D(), gpu_channel_host.get(), attributes,
          lose_context_when_out_of_memory, limits, NULL));
  return context.Pass();
}

void RenderWidget::RegisterRenderFrameProxy(RenderFrameProxy* proxy) {
  render_frame_proxies_.AddObserver(proxy);
}

void RenderWidget::UnregisterRenderFrameProxy(RenderFrameProxy* proxy) {
  render_frame_proxies_.RemoveObserver(proxy);
}

void RenderWidget::RegisterRenderFrame(RenderFrameImpl* frame) {
  render_frames_.AddObserver(frame);
}

void RenderWidget::UnregisterRenderFrame(RenderFrameImpl* frame) {
  render_frames_.RemoveObserver(frame);
}

#if defined(VIDEO_HOLE)
void RenderWidget::RegisterVideoHoleFrame(RenderFrameImpl* frame) {
  video_hole_frames_.AddObserver(frame);
}

void RenderWidget::UnregisterVideoHoleFrame(RenderFrameImpl* frame) {
  video_hole_frames_.RemoveObserver(frame);
}
#endif  // defined(VIDEO_HOLE)

}  // namespace content
