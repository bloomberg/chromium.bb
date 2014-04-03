// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/debug/trace_event_synthetic_delay.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/output/output_surface.h"
#include "cc/trees/layer_tree_host.h"
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
#include "content/renderer/gpu/compositor_software_output_device.h"
#include "content/renderer/gpu/delegated_compositor_output_surface.h"
#include "content/renderer/gpu/mailbox_output_surface.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "content/renderer/resizing_mode_selector.h"
#include "ipc/ipc_sync_message.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDeviceEmulationParams.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebPopupMenu.h"
#include "third_party/WebKit/public/web/WebPopupMenuInfo.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_switches.h"
#include "ui/surface/transport_dib.h"

#if defined(OS_ANDROID)
#include <android/keycodes.h>
#include "base/android/sys_utils.h"
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
using blink::WebPagePopup;
using blink::WebPopupMenu;
using blink::WebPopupMenuInfo;
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
    return Singleton<TextInputModeMapSingleton>::get();
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

  friend struct DefaultSingletonTraits<TextInputModeMapSingleton>;

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
  gfx::Point offset() { return offset_; }
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

 private:
  void Reapply();
  void Apply(float overdraw_bottom_height,
      gfx::Rect resizer_rect, bool is_fullscreen);

  RenderWidget* widget_;

  // Parameters as passed by RenderWidget::EnableScreenMetricsEmulation.
  WebDeviceEmulationParams params_;

  // The computed scale and offset used to fit widget into browser window.
  float scale_;
  gfx::Point offset_;

  // Widget rect as passed to webwidget.
  gfx::Rect applied_widget_rect_;

  // Original values to restore back after emulation ends.
  gfx::Size original_size_;
  gfx::Size original_physical_backing_size_;
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
  original_screen_info_ = widget_->screen_info_;
  original_view_screen_rect_ = widget_->view_screen_rect_;
  original_window_screen_rect_ = widget_->window_screen_rect_;
  Apply(widget_->overdraw_bottom_height_,
        widget_->resizer_rect_, widget_->is_fullscreen_);
}

RenderWidget::ScreenMetricsEmulator::~ScreenMetricsEmulator() {
  widget_->screen_info_ = original_screen_info_;

  widget_->SetDeviceScaleFactor(original_screen_info_.deviceScaleFactor);
  widget_->SetScreenMetricsEmulationParameters(0.f, gfx::Point(), 1.f);
  widget_->view_screen_rect_ = original_view_screen_rect_;
  widget_->window_screen_rect_ = original_window_screen_rect_;
  widget_->Resize(original_size_, original_physical_backing_size_,
      widget_->overdraw_bottom_height_, widget_->resizer_rect_,
      widget_->is_fullscreen_, NO_RESIZE_ACK);
}

void RenderWidget::ScreenMetricsEmulator::ChangeEmulationParams(
    const WebDeviceEmulationParams& params) {
  params_ = params;
  Reapply();
}

void RenderWidget::ScreenMetricsEmulator::Reapply() {
  Apply(widget_->overdraw_bottom_height_,
        widget_->resizer_rect_, widget_->is_fullscreen_);
}

void RenderWidget::ScreenMetricsEmulator::Apply(
    float overdraw_bottom_height, gfx::Rect resizer_rect, bool is_fullscreen) {
  applied_widget_rect_.set_size(params_.viewSize.isEmpty() ?
      original_size_ : gfx::Size(params_.viewSize));

  if (params_.fitToView) {
    DCHECK(!original_size_.IsEmpty());

    int width_with_gutter =
        std::max(original_size_.width() - 2 * params_.viewInsets.width, 1);
    int height_with_gutter =
        std::max(original_size_.height() - 2 * params_.viewInsets.height, 1);
    float width_ratio =
        static_cast<float>(applied_widget_rect_.width()) / width_with_gutter;
    float height_ratio =
        static_cast<float>(applied_widget_rect_.height()) / height_with_gutter;
    float ratio = std::max(1.0f, std::max(width_ratio, height_ratio));
    scale_ = 1.f / ratio;

    // Center emulated view inside available view space.
    offset_.set_x(
        (original_size_.width() - scale_ * applied_widget_rect_.width()) / 2);
    offset_.set_y(
        (original_size_.height() - scale_ * applied_widget_rect_.height()) / 2);
  } else {
    scale_ = 1.f;
    offset_.SetPoint(0, 0);
  }

  if (params_.screenPosition == WebDeviceEmulationParams::Desktop) {
    applied_widget_rect_.set_origin(original_view_screen_rect_.origin());
    widget_->screen_info_.rect = original_screen_info_.rect;
    widget_->screen_info_.availableRect = original_screen_info_.availableRect;
    widget_->window_screen_rect_ = original_window_screen_rect_;
  } else {
    applied_widget_rect_.set_origin(gfx::Point(0, 0));
    widget_->screen_info_.rect = applied_widget_rect_;
    widget_->screen_info_.availableRect = applied_widget_rect_;
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
  widget_->SetScreenMetricsEmulationParameters(
      original_screen_info_.deviceScaleFactor, offset_, scale_);

  widget_->SetDeviceScaleFactor(applied_device_scale_factor);
  widget_->view_screen_rect_ = applied_widget_rect_;

  gfx::Size physical_backing_size = gfx::ToCeiledSize(gfx::ScaleSize(
      original_size_, original_screen_info_.deviceScaleFactor));
  widget_->Resize(applied_widget_rect_.size(), physical_backing_size,
      overdraw_bottom_height, resizer_rect, is_fullscreen, NO_RESIZE_ACK);
}

void RenderWidget::ScreenMetricsEmulator::OnResizeMessage(
    const ViewMsg_Resize_Params& params) {
  bool need_ack = params.new_size != original_size_ &&
      !params.new_size.IsEmpty() && !params.physical_backing_size.IsEmpty();
  original_size_ = params.new_size;
  original_physical_backing_size_ = params.physical_backing_size;
  original_screen_info_ = params.screen_info;
  Apply(params.overdraw_bottom_height, params.resizer_rect,
        params.is_fullscreen);

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

// RenderWidget ---------------------------------------------------------------

RenderWidget::RenderWidget(blink::WebPopupType popup_type,
                           const blink::WebScreenInfo& screen_info,
                           bool swapped_out,
                           bool hidden)
    : routing_id_(MSG_ROUTING_NONE),
      surface_id_(0),
      webwidget_(NULL),
      opener_id_(MSG_ROUTING_NONE),
      init_complete_(false),
      current_paint_buf_(NULL),
      overdraw_bottom_height_(0.f),
      next_paint_flags_(0),
      filtered_time_per_frame_(0.0f),
      update_reply_pending_(false),
      auto_resize_mode_(false),
      need_update_rect_for_auto_resize_(false),
      using_asynchronous_swapbuffers_(false),
      num_swapbuffers_complete_pending_(0),
      did_show_(false),
      is_hidden_(hidden),
      is_fullscreen_(false),
      needs_repainting_on_restore_(false),
      has_focus_(false),
      handling_input_event_(false),
      handling_ime_event_(false),
      handling_touchstart_event_(false),
      closing_(false),
      is_swapped_out_(swapped_out),
      input_method_is_active_(false),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      text_input_mode_(ui::TEXT_INPUT_MODE_DEFAULT),
      can_compose_inline_(true),
      popup_type_(popup_type),
      pending_window_rect_count_(0),
      suppress_next_char_events_(false),
      is_accelerated_compositing_active_(false),
      was_accelerated_compositing_ever_active_(false),
      animation_update_pending_(false),
      invalidation_task_posted_(false),
      screen_info_(screen_info),
      device_scale_factor_(screen_info_.deviceScaleFactor),
      is_threaded_compositing_enabled_(false),
      next_output_surface_id_(0),
#if defined(OS_ANDROID)
      outstanding_ime_acks_(0),
#endif
      popup_origin_scale_for_emulation_(0.f),
      resizing_mode_selector_(new ResizingModeSelector()),
      context_menu_source_type_(ui::MENU_SOURCE_MOUSE) {
  if (!swapped_out)
    RenderProcess::current()->AddRefProcess();
  DCHECK(RenderThread::Get());
  has_disable_gpu_vsync_switch_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGpuVsync);
  is_threaded_compositing_enabled_ =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableThreadedCompositing);

  legacy_software_mode_stats_ = cc::RenderingStatsInstrumentation::Create();
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          cc::switches::kEnableGpuBenchmarking))
    legacy_software_mode_stats_->set_record_rendering_stats(true);
}

RenderWidget::~RenderWidget() {
  DCHECK(!webwidget_) << "Leaking our WebWidget!";
  STLDeleteElements(&updates_pending_swap_);
  if (current_paint_buf_) {
    if (RenderProcess::current()) {
      // If the RenderProcess is already gone, it will have released all DIBs
      // in its destructor anyway.
      RenderProcess::current()->ReleaseTransportDIB(current_paint_buf_);
    }
    current_paint_buf_ = NULL;
  }

  // If we are swapped out, we have released already.
  if (!is_swapped_out_ && RenderProcess::current())
    RenderProcess::current()->ReleaseProcess();
}

// static
RenderWidget* RenderWidget::Create(int32 opener_id,
                                   blink::WebPopupType popup_type,
                                   const blink::WebScreenInfo& screen_info) {
  DCHECK(opener_id != MSG_ROUTING_NONE);
  scoped_refptr<RenderWidget> widget(
      new RenderWidget(popup_type, screen_info, false, false));
  if (widget->Init(opener_id)) {  // adds reference on success.
    return widget.get();
  }
  return NULL;
}

// static
WebWidget* RenderWidget::CreateWebWidget(RenderWidget* render_widget) {
  switch (render_widget->popup_type_) {
    case blink::WebPopupTypeNone:  // Nothing to create.
      break;
    case blink::WebPopupTypeSelect:
    case blink::WebPopupTypeSuggestion:
      return WebPopupMenu::create(render_widget);
    case blink::WebPopupTypePage:
      return WebPagePopup::create(render_widget);
    default:
      NOTREACHED();
  }
  return NULL;
}

bool RenderWidget::Init(int32 opener_id) {
  return DoInit(opener_id,
                RenderWidget::CreateWebWidget(this),
                new ViewHostMsg_CreateWidget(opener_id, popup_type_,
                                             &routing_id_, &surface_id_));
}

bool RenderWidget::DoInit(int32 opener_id,
                          WebWidget* web_widget,
                          IPC::SyncMessage* create_widget_message) {
  DCHECK(!webwidget_);

  if (opener_id != MSG_ROUTING_NONE)
    opener_id_ = opener_id;

  webwidget_ = web_widget;

  bool result = RenderThread::Get()->Send(create_widget_message);
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

// This is used to complete pending inits and non-pending inits.
void RenderWidget::CompleteInit() {
  DCHECK(routing_id_ != MSG_ROUTING_NONE);

  init_complete_ = true;

  if (webwidget_ && is_threaded_compositing_enabled_) {
    webwidget_->enterForceCompositingMode(true);
  }
  if (compositor_) {
    compositor_->setSurfaceReady();
  }
  DoDeferredUpdate();

  Send(new ViewHostMsg_RenderViewReady(routing_id_));
}

void RenderWidget::SetSwappedOut(bool is_swapped_out) {
  // We should only toggle between states.
  DCHECK(is_swapped_out_ != is_swapped_out);
  is_swapped_out_ = is_swapped_out;

  // If we are swapping out, we will call ReleaseProcess, allowing the process
  // to exit if all of its RenderViews are swapped out.  We wait until the
  // WasSwappedOut call to do this, to avoid showing the sad tab.
  // If we are swapping in, we call AddRefProcess to prevent the process from
  // exiting.
  if (!is_swapped_out)
    RenderProcess::current()->AddRefProcess();
}

bool RenderWidget::UsingSynchronousRendererCompositor() const {
#if defined(OS_ANDROID)
  return SynchronousCompositorFactory::GetInstance() != NULL;
#else
  return false;
#endif
}

void RenderWidget::EnableScreenMetricsEmulation(
    const WebDeviceEmulationParams& params) {
  if (!screen_metrics_emulator_)
    screen_metrics_emulator_.reset(new ScreenMetricsEmulator(this, params));
  else
    screen_metrics_emulator_->ChangeEmulationParams(params);
}

void RenderWidget::DisableScreenMetricsEmulation() {
  screen_metrics_emulator_.reset();
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

void RenderWidget::SetScreenMetricsEmulationParameters(
    float device_scale_factor,
    const gfx::Point& root_layer_offset,
    float root_layer_scale) {
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

void RenderWidget::ScheduleCompositeWithForcedRedraw() {
  if (compositor_) {
    // Regardless of whether threaded compositing is enabled, always
    // use this mechanism to force the compositor to redraw. However,
    // the invalidation code path below is still needed for the
    // non-threaded case.
    compositor_->SetNeedsForcedRedraw();
  }
  scheduleComposite();
}

bool RenderWidget::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidget, message)
    IPC_MESSAGE_HANDLER(InputMsg_HandleInputEvent, OnHandleInputEvent)
    IPC_MESSAGE_HANDLER(InputMsg_CursorVisibilityChange,
                        OnCursorVisibilityChange)
    IPC_MESSAGE_HANDLER(InputMsg_MouseCaptureLost, OnMouseCaptureLost)
    IPC_MESSAGE_HANDLER(InputMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(InputMsg_SyntheticGestureCompleted,
                        OnSyntheticGestureCompleted)
    IPC_MESSAGE_HANDLER(ViewMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(ViewMsg_CreatingNew_ACK, OnCreatingNewAck)
    IPC_MESSAGE_HANDLER(ViewMsg_Resize, OnResize)
    IPC_MESSAGE_HANDLER(ViewMsg_ChangeResizeRect, OnChangeResizeRect)
    IPC_MESSAGE_HANDLER(ViewMsg_WasHidden, OnWasHidden)
    IPC_MESSAGE_HANDLER(ViewMsg_WasShown, OnWasShown)
    IPC_MESSAGE_HANDLER(ViewMsg_WasSwappedOut, OnWasSwappedOut)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateRect_ACK, OnUpdateRectAck)
    IPC_MESSAGE_HANDLER(ViewMsg_SwapBuffers_ACK, OnSwapBuffersComplete)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInputMethodActive, OnSetInputMethodActive)
    IPC_MESSAGE_HANDLER(ViewMsg_CandidateWindowShown, OnCandidateWindowShown)
    IPC_MESSAGE_HANDLER(ViewMsg_CandidateWindowUpdated,
                        OnCandidateWindowUpdated)
    IPC_MESSAGE_HANDLER(ViewMsg_CandidateWindowHidden, OnCandidateWindowHidden)
    IPC_MESSAGE_HANDLER(ViewMsg_ImeSetComposition, OnImeSetComposition)
    IPC_MESSAGE_HANDLER(ViewMsg_ImeConfirmComposition, OnImeConfirmComposition)
    IPC_MESSAGE_HANDLER(ViewMsg_Repaint, OnRepaint)
    IPC_MESSAGE_HANDLER(ViewMsg_SetTextDirection, OnSetTextDirection)
    IPC_MESSAGE_HANDLER(ViewMsg_Move_ACK, OnRequestMoveAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateScreenRects, OnUpdateScreenRects)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewMsg_ShowImeIfNeeded, OnShowImeIfNeeded)
    IPC_MESSAGE_HANDLER(ViewMsg_ImeEventAck, OnImeEventAck)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_Snapshot, OnSnapshot)
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
                          float overdraw_bottom_height,
                          const gfx::Rect& resizer_rect,
                          bool is_fullscreen,
                          ResizeAck resize_ack) {
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
    compositor_->SetOverdrawBottomHeight(overdraw_bottom_height);
  }

  physical_backing_size_ = physical_backing_size;
  overdraw_bottom_height_ = overdraw_bottom_height;
  resizer_rect_ = resizer_rect;

  // NOTE: We may have entered fullscreen mode without changing our size.
  bool fullscreen_change = is_fullscreen_ != is_fullscreen;
  if (fullscreen_change)
    WillToggleFullscreen();
  is_fullscreen_ = is_fullscreen;

  if (size_ != new_size) {
    // TODO(darin): We should not need to reset this here.
    needs_repainting_on_restore_ = false;

    size_ = new_size;

    paint_aggregator_.ClearPendingUpdate();

    // When resizing, we want to wait to paint before ACK'ing the resize.  This
    // ensures that we only resize as fast as we can paint.  We only need to
    // send an ACK if we are resized to a non-empty rect.
    webwidget_->resize(new_size);

    if (resizing_mode_selector_->NeverUsesSynchronousResize()) {
      // Resize should have caused an invalidation of the entire view.
      DCHECK(new_size.IsEmpty() || is_accelerated_compositing_active_ ||
             paint_aggregator_.HasPendingUpdate());
    }
  } else if (!resizing_mode_selector_->is_synchronous_mode()) {
    resize_ack = NO_RESIZE_ACK;
  }

  if (new_size.IsEmpty() || physical_backing_size.IsEmpty()) {
    // For empty size or empty physical_backing_size, there is no next paint
    // (along with which to send the ack) until they are set to non-empty.
    resize_ack = NO_RESIZE_ACK;
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

void RenderWidget::ResizeSynchronously(const gfx::Rect& new_position) {
  Resize(new_position.size(), new_position.size(), overdraw_bottom_height_,
         gfx::Rect(), is_fullscreen_, NO_RESIZE_ACK);
  view_screen_rect_ = new_position;
  window_screen_rect_ = new_position;
  if (!did_show_)
    initial_pos_ = new_position;
}

void RenderWidget::OnClose() {
  if (closing_)
    return;
  closing_ = true;

  // Browser correspondence is no longer needed at this point.
  if (routing_id_ != MSG_ROUTING_NONE) {
    if (RenderThreadImpl::current())
      RenderThreadImpl::current()->WidgetDestroyed();
    RenderThread::Get()->RemoveRoute(routing_id_);
    SetHidden(false);
  }

  // If there is a Send call on the stack, then it could be dangerous to close
  // now.  Post a task that only gets invoked when there are no nested message
  // loops.
  base::MessageLoop::current()->PostNonNestableTask(
      FROM_HERE, base::Bind(&RenderWidget::Close, this));

  // Balances the AddRef taken when we called AddRoute.
  Release();
}

// Got a response from the browser after the renderer decided to create a new
// view.
void RenderWidget::OnCreatingNewAck() {
  DCHECK(routing_id_ != MSG_ROUTING_NONE);

  CompleteInit();
}

void RenderWidget::OnResize(const ViewMsg_Resize_Params& params) {
  if (resizing_mode_selector_->ShouldAbortOnResize(this, params))
    return;

  if (screen_metrics_emulator_) {
    screen_metrics_emulator_->OnResizeMessage(params);
    return;
  }

  screen_info_ = params.screen_info;
  SetDeviceScaleFactor(screen_info_.deviceScaleFactor);
  Resize(params.new_size, params.physical_backing_size,
         params.overdraw_bottom_height, params.resizer_rect,
         params.is_fullscreen, SEND_RESIZE_ACK);
}

void RenderWidget::OnChangeResizeRect(const gfx::Rect& resizer_rect) {
  if (resizer_rect_ != resizer_rect) {
    gfx::Rect view_rect(size_);

    gfx::Rect old_damage_rect = gfx::IntersectRects(view_rect, resizer_rect_);
    if (!old_damage_rect.IsEmpty())
      paint_aggregator_.InvalidateRect(old_damage_rect);

    gfx::Rect new_damage_rect = gfx::IntersectRects(view_rect, resizer_rect);
    if (!new_damage_rect.IsEmpty())
      paint_aggregator_.InvalidateRect(new_damage_rect);

    resizer_rect_ = resizer_rect;

    if (webwidget_)
      webwidget_->didChangeWindowResizerRect();
  }
}

void RenderWidget::OnWasHidden() {
  TRACE_EVENT0("renderer", "RenderWidget::OnWasHidden");
  // Go into a mode where we stop generating paint and scrolling events.
  SetHidden(true);
}

void RenderWidget::OnWasShown(bool needs_repainting) {
  TRACE_EVENT0("renderer", "RenderWidget::OnWasShown");
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  // See OnWasHidden
  SetHidden(false);

  if (!needs_repainting && !needs_repainting_on_restore_)
    return;
  needs_repainting_on_restore_ = false;

  // Tag the next paint as a restore ack, which is picked up by
  // DoDeferredUpdate when it sends out the next PaintRect message.
  set_next_paint_is_restore_ack();

  // Generate a full repaint.
  if (!is_accelerated_compositing_active_) {
    didInvalidateRect(gfx::Rect(size_.width(), size_.height()));
  } else {
    if (compositor_)
      compositor_->SetNeedsForcedRedraw();
    scheduleComposite();
  }
}

void RenderWidget::OnWasSwappedOut() {
  // If we have been swapped out and no one else is using this process,
  // it's safe to exit now.  If we get swapped back in, we will call
  // AddRefProcess in SetSwappedOut.
  if (is_swapped_out_)
    RenderProcess::current()->ReleaseProcess();
}

void RenderWidget::OnRequestMoveAck() {
  DCHECK(pending_window_rect_count_);
  pending_window_rect_count_--;
}

void RenderWidget::OnUpdateRectAck() {
  TRACE_EVENT0("renderer", "RenderWidget::OnUpdateRectAck");
  DCHECK(update_reply_pending_);
  update_reply_pending_ = false;

  // If we sent an UpdateRect message with a zero-sized bitmap, then we should
  // have no current paint buffer.
  if (current_paint_buf_) {
    RenderProcess::current()->ReleaseTransportDIB(current_paint_buf_);
    current_paint_buf_ = NULL;
  }

  // If swapbuffers is still pending, then defer the update until the
  // swapbuffers occurs.
  if (num_swapbuffers_complete_pending_ >= kMaxSwapBuffersPending) {
    TRACE_EVENT0("renderer", "EarlyOut_SwapStillPending");
    return;
  }

  // Notify subclasses that software rendering was flushed to the screen.
  if (!is_accelerated_compositing_active_) {
    DidFlushPaint();
  }

  // Continue painting if necessary...
  DoDeferredUpdateAndSendInputAck();
}

bool RenderWidget::SupportsAsynchronousSwapBuffers() {
  // Contexts using the command buffer support asynchronous swapbuffers.
  // See RenderWidget::CreateOutputSurface().
  if (RenderThreadImpl::current()->compositor_message_loop_proxy().get())
    return false;

  return true;
}

GURL RenderWidget::GetURLForGraphicsContext3D() {
  return GURL();
}

bool RenderWidget::ForceCompositingModeEnabled() {
  return false;
}

scoped_ptr<cc::OutputSurface> RenderWidget::CreateOutputSurface(bool fallback) {

#if defined(OS_ANDROID)
  if (SynchronousCompositorFactory* factory =
      SynchronousCompositorFactory::GetInstance()) {
    return factory->CreateOutputSurface(routing_id());
  }
#endif

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool use_software = fallback;
  if (command_line.HasSwitch(switches::kDisableGpuCompositing))
    use_software = true;

  scoped_refptr<ContextProviderCommandBuffer> context_provider;
  if (!use_software) {
    context_provider = ContextProviderCommandBuffer::Create(
        CreateGraphicsContext3D(), "RenderCompositor");
    if (!context_provider.get()) {
      // Cause the compositor to wait and try again.
      return scoped_ptr<cc::OutputSurface>();
    }
  }

  uint32 output_surface_id = next_output_surface_id_++;
  if (command_line.HasSwitch(switches::kEnableDelegatedRenderer)) {
    DCHECK(is_threaded_compositing_enabled_);
    return scoped_ptr<cc::OutputSurface>(
        new DelegatedCompositorOutputSurface(
            routing_id(),
            output_surface_id,
            context_provider));
  }
  if (!context_provider.get()) {
    if (!command_line.HasSwitch(switches::kEnableSoftwareCompositing))
      return scoped_ptr<cc::OutputSurface>();

    scoped_ptr<cc::SoftwareOutputDevice> software_device(
        new CompositorSoftwareOutputDevice());

    return scoped_ptr<cc::OutputSurface>(new CompositorOutputSurface(
        routing_id(),
        output_surface_id,
        NULL,
        software_device.Pass(),
        true));
  }

  if (command_line.HasSwitch(cc::switches::kCompositeToMailbox)) {
    DCHECK(is_threaded_compositing_enabled_);
    cc::ResourceFormat format = cc::RGBA_8888;
#if defined(OS_ANDROID)
    if (base::android::SysUtils::IsLowEndDevice())
      format = cc::RGB_565;
#endif
    return scoped_ptr<cc::OutputSurface>(
        new MailboxOutputSurface(
            routing_id(),
            output_surface_id,
            context_provider,
            scoped_ptr<cc::SoftwareOutputDevice>(),
            format));
  }
  bool use_swap_compositor_frame_message = false;
  return scoped_ptr<cc::OutputSurface>(
      new CompositorOutputSurface(
          routing_id(),
          output_surface_id,
          context_provider,
          scoped_ptr<cc::SoftwareOutputDevice>(),
          use_swap_compositor_frame_message));
}

void RenderWidget::OnSwapBuffersAborted() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersAborted");
  while (!updates_pending_swap_.empty()) {
    ViewHostMsg_UpdateRect* msg = updates_pending_swap_.front();
    updates_pending_swap_.pop_front();
    // msg can be NULL if the swap doesn't correspond to an DoDeferredUpdate
    // compositing pass, hence doesn't require an UpdateRect message.
    if (msg)
      Send(msg);
  }
  num_swapbuffers_complete_pending_ = 0;
  using_asynchronous_swapbuffers_ = false;
  // Schedule another frame so the compositor learns about it.
  scheduleComposite();
}

void RenderWidget::OnSwapBuffersPosted() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersPosted");

  if (using_asynchronous_swapbuffers_) {
    ViewHostMsg_UpdateRect* msg = NULL;
    // pending_update_params_ can be NULL if the swap doesn't correspond to an
    // DoDeferredUpdate compositing pass, hence doesn't require an UpdateRect
    // message.
    if (pending_update_params_) {
      msg = new ViewHostMsg_UpdateRect(routing_id_, *pending_update_params_);
      pending_update_params_.reset();
    }
    updates_pending_swap_.push_back(msg);
    num_swapbuffers_complete_pending_++;
  }
}

void RenderWidget::OnSwapBuffersComplete() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersComplete");

  // Notify subclasses that composited rendering was flushed to the screen.
  DidFlushPaint();

  // When compositing deactivates, we reset the swapbuffers pending count.  The
  // swapbuffers acks may still arrive, however.
  if (num_swapbuffers_complete_pending_ == 0) {
    TRACE_EVENT0("renderer", "EarlyOut_ZeroSwapbuffersPending");
    return;
  }
  DCHECK(!updates_pending_swap_.empty());
  ViewHostMsg_UpdateRect* msg = updates_pending_swap_.front();
  updates_pending_swap_.pop_front();
  // msg can be NULL if the swap doesn't correspond to an DoDeferredUpdate
  // compositing pass, hence doesn't require an UpdateRect message.
  if (msg)
    Send(msg);
  num_swapbuffers_complete_pending_--;

  // If update reply is still pending, then defer the update until that reply
  // occurs.
  if (update_reply_pending_) {
    TRACE_EVENT0("renderer", "EarlyOut_UpdateReplyPending");
    return;
  }

  // If we are not accelerated rendering, then this is a stale swapbuffers from
  // when we were previously rendering. However, if an invalidation task is not
  // posted, there may be software rendering work pending. In that case, don't
  // early out.
  if (!is_accelerated_compositing_active_ && invalidation_task_posted_) {
    TRACE_EVENT0("renderer", "EarlyOut_AcceleratedCompositingOff");
    return;
  }

  // Do not call DoDeferredUpdate unless there's animation work to be done or
  // a real invalidation. This prevents rendering in response to a swapbuffers
  // callback coming back after we've navigated away from the page that
  // generated it.
  if (!animation_update_pending_ && !paint_aggregator_.HasPendingUpdate()) {
    TRACE_EVENT0("renderer", "EarlyOut_NoPendingUpdate");
    return;
  }

  // Continue painting if necessary...
  DoDeferredUpdateAndSendInputAck();
}

void RenderWidget::OnHandleInputEvent(const blink::WebInputEvent* input_event,
                                      const ui::LatencyInfo& latency_info,
                                      bool is_keyboard_shortcut) {
  handling_input_event_ = true;
  if (!input_event) {
    handling_input_event_ = false;
    return;
  }

  base::TimeTicks start_time;
  if (base::TimeTicks::IsHighResNowFastAndReliable())
    start_time = base::TimeTicks::HighResNow();

  const char* const event_name =
      WebInputEventTraits::GetName(input_event->type);
  TRACE_EVENT1("renderer", "RenderWidget::OnHandleInputEvent",
               "event", event_name);
  TRACE_EVENT_SYNTHETIC_DELAY_BEGIN("blink.HandleInputEvent");
  TRACE_EVENT_FLOW_STEP0(
      "input",
      "LatencyInfo.Flow",
      TRACE_ID_DONT_MANGLE(latency_info.trace_id),
      "HanldeInputEventMain");

  scoped_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor;
  ui::LatencyInfo swap_latency_info(latency_info);
  if (compositor_) {
    latency_info_swap_promise_monitor =
        compositor_->CreateLatencyInfoSwapPromiseMonitor(&swap_latency_info)
            .Pass();
  } else {
    latency_info_.push_back(latency_info);
  }

  base::TimeDelta now = base::TimeDelta::FromInternalValue(
      base::TimeTicks::Now().ToInternalValue());

  int64 delta = static_cast<int64>(
      (now.InSecondsF() - input_event->timeStampSeconds) *
          base::Time::kMicrosecondsPerSecond);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Event.AggregatedLatency.Renderer2",
                              delta, 1, 10000000, 100);
  base::HistogramBase* counter_for_type =
      base::Histogram::FactoryGet(
          base::StringPrintf("Event.Latency.Renderer2.%s", event_name),
          1,
          10000000,
          100,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  counter_for_type->Add(delta);

  if (WebInputEvent::isUserGestureEventType(input_event->type))
    WillProcessUserGesture();

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

  if (input_event->type == WebInputEvent::TouchStart)
      handling_touchstart_event_ = true;

  bool processed = prevent_default;
  if (input_event->type != WebInputEvent::Char || !suppress_next_char_events_) {
    suppress_next_char_events_ = false;
    if (!processed && webwidget_)
      processed = webwidget_->handleInputEvent(*input_event);
  }

  handling_touchstart_event_ = false;

  // If this RawKeyDown event corresponds to a browser keyboard shortcut and
  // it's not processed by webkit, then we need to suppress the upcoming Char
  // events.
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

  bool event_type_can_be_rate_limited =
      input_event->type == WebInputEvent::MouseMove ||
      input_event->type == WebInputEvent::MouseWheel ||
      input_event->type == WebInputEvent::TouchMove;

  bool frame_pending = paint_aggregator_.HasPendingUpdate();
  if (is_accelerated_compositing_active_) {
    frame_pending = compositor_ &&
                    compositor_->BeginMainFrameRequested();
  }

  // If we don't have a fast and accurate HighResNow, we assume the input
  // handlers are heavy and rate limit them.
  bool rate_limiting_wanted = true;
  if (base::TimeTicks::IsHighResNowFastAndReliable()) {
      base::TimeTicks end_time = base::TimeTicks::HighResNow();
      total_input_handling_time_this_frame_ += (end_time - start_time);
      rate_limiting_wanted =
          total_input_handling_time_this_frame_.InMicroseconds() >
          kInputHandlingTimeThrottlingThresholdMicroseconds;
  }

  if (!WebInputEventTraits::IgnoresAckDisposition(input_event->type)) {
    scoped_ptr<IPC::Message> response(
        new InputHostMsg_HandleInputEvent_ACK(routing_id_,
                                              input_event->type,
                                              ack_result,
                                              swap_latency_info));
    if (rate_limiting_wanted && event_type_can_be_rate_limited &&
        frame_pending && !is_hidden_) {
      // We want to rate limit the input events in this case, so we'll wait for
      // painting to finish before ACKing this message.
      TRACE_EVENT_INSTANT0("renderer",
        "RenderWidget::OnHandleInputEvent ack throttled",
        TRACE_EVENT_SCOPE_THREAD);
      if (pending_input_event_ack_) {
        // As two different kinds of events could cause us to postpone an ack
        // we send it now, if we have one pending. The Browser should never
        // send us the same kind of event we are delaying the ack for.
        Send(pending_input_event_ack_.release());
      }
      pending_input_event_ack_ = response.Pass();
      if (compositor_)
        compositor_->NotifyInputThrottledUntilCommit();
    } else {
      Send(response.release());
    }
  }

#if defined(OS_ANDROID)
  // Allow the IME to be shown when the focus changes as a consequence
  // of a processed touch end event.
  if (input_event->type == WebInputEvent::TouchEnd && processed)
    UpdateTextInputState(true, true);
#elif defined(USE_AURA)
  // Show the virtual keyboard if enabled and a user gesture triggers a focus
  // change.
  if (processed && (input_event->type == WebInputEvent::TouchEnd ||
      input_event->type == WebInputEvent::MouseUp))
    UpdateTextInputState(true, false);
#endif

  TRACE_EVENT_SYNTHETIC_DELAY_END("blink.HandleInputEvent");
  handling_input_event_ = false;

  if (!prevent_default) {
    if (WebInputEvent::isKeyboardEventType(input_event->type))
      DidHandleKeyEvent();
    if (WebInputEvent::isMouseEventType(input_event->type))
      DidHandleMouseEvent(*(static_cast<const WebMouseEvent*>(input_event)));
    if (WebInputEvent::isTouchEventType(input_event->type))
      DidHandleTouchEvent(*(static_cast<const WebTouchEvent*>(input_event)));
  }
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
  has_focus_ = enable;
  if (webwidget_)
    webwidget_->setFocus(enable);
}

void RenderWidget::ClearFocus() {
  // We may have got the focus from the browser before this gets processed, in
  // which case we do not want to unfocus ourself.
  if (!has_focus_ && webwidget_)
    webwidget_->setFocus(false);
}

void RenderWidget::PaintRect(const gfx::Rect& rect,
                             const gfx::Point& canvas_origin,
                             skia::PlatformCanvas* canvas) {
  TRACE_EVENT2("renderer", "PaintRect",
               "width", rect.width(), "height", rect.height());

  canvas->save();

  // Bring the canvas into the coordinate system of the paint rect.
  canvas->translate(static_cast<SkScalar>(-canvas_origin.x()),
                    static_cast<SkScalar>(-canvas_origin.y()));

  // If there is a custom background, tile it.
  if (!background_.empty()) {
    SkPaint paint;
    skia::RefPtr<SkShader> shader = skia::AdoptRef(
        SkShader::CreateBitmapShader(background_,
                                     SkShader::kRepeat_TileMode,
                                     SkShader::kRepeat_TileMode));
    paint.setShader(shader.get());

    // Use kSrc_Mode to handle background_ transparency properly.
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);

    // Canvas could contain multiple update rects. Clip to given rect so that
    // we don't accidentally clear other update rects.
    canvas->save();
    canvas->scale(device_scale_factor_, device_scale_factor_);
    canvas->clipRect(gfx::RectToSkRect(rect));
    canvas->drawPaint(paint);
    canvas->restore();
  }

  // First see if this rect is a plugin that can paint itself faster.
  TransportDIB* optimized_dib = NULL;
  gfx::Rect optimized_copy_rect, optimized_copy_location;
  float dib_scale_factor;
  PepperPluginInstanceImpl* optimized_instance =
      GetBitmapForOptimizedPluginPaint(rect, &optimized_dib,
                                       &optimized_copy_location,
                                       &optimized_copy_rect,
                                       &dib_scale_factor);
  if (optimized_instance) {
#if defined(ENABLE_PLUGINS)
    // This plugin can be optimize-painted and we can just ask it to paint
    // itself. We don't actually need the TransportDIB in this case.
    //
    // This is an optimization for PPAPI plugins that know they're on top of
    // the page content. If this rect is inside such a plugin, we can save some
    // time and avoid re-rendering the page content which we know will be
    // covered by the plugin later (this time can be significant, especially
    // for a playing movie that is invalidating a lot).
    //
    // In the plugin movie case, hopefully the similar call to
    // GetBitmapForOptimizedPluginPaint in DoDeferredUpdate handles the
    // painting, because that avoids copying the plugin image to a different
    // paint rect. Unfortunately, if anything on the page is animating other
    // than the movie, it break this optimization since the union of the
    // invalid regions will be larger than the plugin.
    //
    // This code optimizes that case, where we can still avoid painting in
    // WebKit and filling the background (which can be slow) and just painting
    // the plugin. Unlike the DoDeferredUpdate case, an extra copy is still
    // required.
    SkAutoCanvasRestore auto_restore(canvas, true);
    canvas->scale(device_scale_factor_, device_scale_factor_);
    optimized_instance->Paint(canvas, optimized_copy_location, rect);
    canvas->restore();
#endif
  } else {
    // Normal painting case.
    base::TimeTicks start_time;
    if (!is_accelerated_compositing_active_)
      start_time = legacy_software_mode_stats_->StartRecording();

    webwidget_->paint(canvas, rect);

    if (!is_accelerated_compositing_active_) {
      base::TimeDelta paint_time =
          legacy_software_mode_stats_->EndRecording(start_time);
      int64 painted_pixel_count = rect.width() * rect.height();
      legacy_software_mode_stats_->AddPaint(paint_time, painted_pixel_count);
    }

    // Flush to underlying bitmap.  TODO(darin): is this needed?
    skia::GetTopDevice(*canvas)->accessBitmap(false);
  }

  PaintDebugBorder(rect, canvas);
  canvas->restore();
}

void RenderWidget::PaintDebugBorder(const gfx::Rect& rect,
                                    skia::PlatformCanvas* canvas) {
  static bool kPaintBorder =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kShowPaintRects);
  if (!kPaintBorder)
    return;

  // Cycle through these colors to help distinguish new paint rects.
  const SkColor colors[] = {
    SkColorSetARGB(0x3F, 0xFF, 0, 0),
    SkColorSetARGB(0x3F, 0xFF, 0, 0xFF),
    SkColorSetARGB(0x3F, 0, 0, 0xFF),
  };
  static int color_selector = 0;

  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setColor(colors[color_selector++ % arraysize(colors)]);
  paint.setStrokeWidth(1);

  SkIRect irect;
  irect.set(rect.x(), rect.y(), rect.right() - 1, rect.bottom() - 1);
  canvas->drawIRect(irect, paint);
}

void RenderWidget::AnimationCallback() {
  TRACE_EVENT0("renderer", "RenderWidget::AnimationCallback");
  if (!animation_update_pending_) {
    TRACE_EVENT0("renderer", "EarlyOut_NoAnimationUpdatePending");
    return;
  }
  if (!animation_floor_time_.is_null() && IsRenderingVSynced()) {
    // Record when we fired (according to base::Time::Now()) relative to when
    // we posted the task to quantify how much the base::Time/base::TimeTicks
    // skew is affecting animations.
    base::TimeDelta animation_callback_delay = base::Time::Now() -
        (animation_floor_time_ - base::TimeDelta::FromMilliseconds(16));
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer4.AnimationCallbackDelayTime",
                               animation_callback_delay,
                               base::TimeDelta::FromMilliseconds(0),
                               base::TimeDelta::FromMilliseconds(30),
                               25);
  }
  DoDeferredUpdateAndSendInputAck();
}

void RenderWidget::AnimateIfNeeded() {
  if (!animation_update_pending_)
    return;

  // Target 60FPS if vsync is on. Go as fast as we can if vsync is off.
  base::TimeDelta animationInterval = IsRenderingVSynced() ?
      base::TimeDelta::FromMilliseconds(16) : base::TimeDelta();

  base::Time now = base::Time::Now();

  // animation_floor_time_ is the earliest time that we should animate when
  // using the dead reckoning software scheduler. If we're using swapbuffers
  // complete callbacks to rate limit, we can ignore this floor.
  if (now >= animation_floor_time_ || num_swapbuffers_complete_pending_ > 0) {
    TRACE_EVENT0("renderer", "RenderWidget::AnimateIfNeeded")
    animation_floor_time_ = now + animationInterval;
    // Set a timer to call us back after animationInterval before
    // running animation callbacks so that if a callback requests another
    // we'll be sure to run it at the proper time.
    animation_timer_.Stop();
    animation_timer_.Start(FROM_HERE, animationInterval, this,
                           &RenderWidget::AnimationCallback);
    animation_update_pending_ = false;
    if (is_accelerated_compositing_active_ && compositor_) {
      compositor_->UpdateAnimations(base::TimeTicks::Now());
    } else {
      double frame_begin_time =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
      webwidget_->animate(frame_begin_time);
    }
    return;
  }
  TRACE_EVENT0("renderer", "EarlyOut_AnimatedTooRecently");
  if (!animation_timer_.IsRunning()) {
    // This code uses base::Time::Now() to calculate the floor and next fire
    // time because javascript's Date object uses base::Time::Now().  The
    // message loop uses base::TimeTicks, which on windows can have a
    // different granularity than base::Time.
    // The upshot of all this is that this function might be called before
    // base::Time::Now() has advanced past the animation_floor_time_.  To
    // avoid exposing this delay to javascript, we keep posting delayed
    // tasks until base::Time::Now() has advanced far enough.
    base::TimeDelta delay = animation_floor_time_ - now;
    animation_timer_.Start(FROM_HERE, delay, this,
                           &RenderWidget::AnimationCallback);
  }
}

bool RenderWidget::IsRenderingVSynced() {
  // TODO(nduca): Forcing a driver to disable vsync (e.g. in a control panel) is
  // not caught by this check. This will lead to artificially low frame rates
  // for people who force vsync off at a driver level and expect Chrome to speed
  // up.
  return !has_disable_gpu_vsync_switch_;
}

void RenderWidget::InvalidationCallback() {
  TRACE_EVENT0("renderer", "RenderWidget::InvalidationCallback");
  invalidation_task_posted_ = false;
  DoDeferredUpdateAndSendInputAck();
}

void RenderWidget::FlushPendingInputEventAck() {
  if (pending_input_event_ack_)
    Send(pending_input_event_ack_.release());
  total_input_handling_time_this_frame_ = base::TimeDelta();
}

void RenderWidget::DoDeferredUpdateAndSendInputAck() {
  DoDeferredUpdate();
  FlushPendingInputEventAck();
}

void RenderWidget::DoDeferredUpdate() {
  TRACE_EVENT0("renderer", "RenderWidget::DoDeferredUpdate");
  TRACE_EVENT_SCOPED_SAMPLING_STATE("Chrome", "Paint");

  if (!webwidget_)
    return;

  if (!init_complete_) {
    TRACE_EVENT0("renderer", "EarlyOut_InitNotComplete");
    return;
  }
  if (update_reply_pending_) {
    TRACE_EVENT0("renderer", "EarlyOut_UpdateReplyPending");
    return;
  }
  if (is_accelerated_compositing_active_ &&
      num_swapbuffers_complete_pending_ >= kMaxSwapBuffersPending) {
    TRACE_EVENT0("renderer", "EarlyOut_MaxSwapBuffersPending");
    return;
  }

  // Suppress updating when we are hidden.
  if (is_hidden_ || size_.IsEmpty() || is_swapped_out_) {
    paint_aggregator_.ClearPendingUpdate();
    needs_repainting_on_restore_ = true;
    TRACE_EVENT0("renderer", "EarlyOut_NotVisible");
    return;
  }

  // Tracking of frame rate jitter
  base::TimeTicks frame_begin_ticks = gfx::FrameTime::Now();
  InstrumentWillBeginFrame(0);
  AnimateIfNeeded();

  // Layout may generate more invalidation.  It may also enable the
  // GPU acceleration, so make sure to run layout before we send the
  // GpuRenderingActivated message.
  webwidget_->layout();

  // Check for whether we need to track swap buffers. We need to do that after
  // layout() because it may have switched us to accelerated compositing.
  if (is_accelerated_compositing_active_)
    using_asynchronous_swapbuffers_ = SupportsAsynchronousSwapBuffers();

  // The following two can result in further layout and possibly
  // enable GPU acceleration so they need to be called before any painting
  // is done.
  UpdateTextInputType();
#if defined(OS_ANDROID)
  UpdateSelectionRootBounds();
#endif
  UpdateSelectionBounds();

  // Suppress painting if nothing is dirty.  This has to be done after updating
  // animations running layout as these may generate further invalidations.
  if (!paint_aggregator_.HasPendingUpdate()) {
    TRACE_EVENT0("renderer", "EarlyOut_NoPendingUpdate");
    InstrumentDidCancelFrame();
    return;
  }

  if (!is_accelerated_compositing_active_ &&
      !is_threaded_compositing_enabled_ &&
      (ForceCompositingModeEnabled() ||
          was_accelerated_compositing_ever_active_)) {
    webwidget_->enterForceCompositingMode(true);
  }

  if (!last_do_deferred_update_time_.is_null()) {
    base::TimeDelta delay = frame_begin_ticks - last_do_deferred_update_time_;
    if (is_accelerated_compositing_active_) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Renderer4.AccelDoDeferredUpdateDelay",
                                 delay,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMilliseconds(120),
                                 60);
    } else {
      UMA_HISTOGRAM_CUSTOM_TIMES("Renderer4.SoftwareDoDeferredUpdateDelay",
                                 delay,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMilliseconds(120),
                                 60);
    }

    // Calculate filtered time per frame:
    float frame_time_elapsed = static_cast<float>(delay.InSecondsF());
    filtered_time_per_frame_ =
        0.9f * filtered_time_per_frame_ + 0.1f * frame_time_elapsed;
  }
  last_do_deferred_update_time_ = frame_begin_ticks;

  if (!is_accelerated_compositing_active_) {
    legacy_software_mode_stats_->IncrementFrameCount(1, true);
    cc::BenchmarkInstrumentation::IssueMainThreadRenderingStatsEvent(
        legacy_software_mode_stats_->main_thread_rendering_stats());
    legacy_software_mode_stats_->AccumulateAndClearMainThreadStats();
  }

  // OK, save the pending update to a local since painting may cause more
  // invalidation.  Some WebCore rendering objects only layout when painted.
  PaintAggregator::PendingUpdate update;
  paint_aggregator_.PopPendingUpdate(&update);

  gfx::Rect scroll_damage = update.GetScrollDamage();
  gfx::Rect bounds = gfx::UnionRects(update.GetPaintBounds(), scroll_damage);

  // A plugin may be able to do an optimized paint. First check this, in which
  // case we can skip all of the bitmap generation and regular paint code.
  // This optimization allows PPAPI plugins that declare themselves on top of
  // the page (like a traditional windowed plugin) to be able to animate (think
  // movie playing) without repeatedly re-painting the page underneath, or
  // copying the plugin backing store (since we can send the plugin's backing
  // store directly to the browser).
  //
  // This optimization only works when the entire invalid region is contained
  // within the plugin. There is a related optimization in PaintRect for the
  // case where there may be multiple invalid regions.
  TransportDIB* dib = NULL;
  gfx::Rect optimized_copy_rect, optimized_copy_location;
  float dib_scale_factor = 1;
  DCHECK(!pending_update_params_.get());
  pending_update_params_.reset(new ViewHostMsg_UpdateRect_Params);
  pending_update_params_->scroll_delta = update.scroll_delta;
  pending_update_params_->scroll_rect = update.scroll_rect;
  pending_update_params_->view_size = size_;
  pending_update_params_->plugin_window_moves.swap(plugin_window_moves_);
  pending_update_params_->flags = next_paint_flags_;
  pending_update_params_->scroll_offset = GetScrollOffset();
  pending_update_params_->needs_ack = true;
  pending_update_params_->scale_factor = device_scale_factor_;
  next_paint_flags_ = 0;
  need_update_rect_for_auto_resize_ = false;

  if (!is_accelerated_compositing_active_)
    pending_update_params_->latency_info.swap(latency_info_);

  latency_info_.clear();

  if (update.scroll_rect.IsEmpty() &&
      !is_accelerated_compositing_active_ &&
      GetBitmapForOptimizedPluginPaint(bounds, &dib, &optimized_copy_location,
                                       &optimized_copy_rect,
                                       &dib_scale_factor)) {
    // Only update the part of the plugin that actually changed.
    optimized_copy_rect.Intersect(bounds);
    pending_update_params_->bitmap = dib->id();
    pending_update_params_->bitmap_rect = optimized_copy_location;
    pending_update_params_->copy_rects.push_back(optimized_copy_rect);
    pending_update_params_->scale_factor = dib_scale_factor;
  } else if (!is_accelerated_compositing_active_) {
    // Compute a buffer for painting and cache it.

    bool fractional_scale = device_scale_factor_ -
        static_cast<int>(device_scale_factor_) != 0;
    if (fractional_scale) {
      // Damage might not be DIP aligned. Inflate damage to compensate.
      bounds.Inset(-1, -1);
      bounds.Intersect(gfx::Rect(size_));
    }

    gfx::Rect pixel_bounds = gfx::ToEnclosingRect(
        gfx::ScaleRect(bounds, device_scale_factor_));

    scoped_ptr<skia::PlatformCanvas> canvas(
        RenderProcess::current()->GetDrawingCanvas(&current_paint_buf_,
                                                   pixel_bounds));
    if (!canvas) {
      NOTREACHED();
      return;
    }

    // We may get back a smaller canvas than we asked for.
    // TODO(darin): This seems like it could cause painting problems!
    DCHECK_EQ(pixel_bounds.width(), canvas->getDevice()->width());
    DCHECK_EQ(pixel_bounds.height(), canvas->getDevice()->height());
    pixel_bounds.set_width(canvas->getDevice()->width());
    pixel_bounds.set_height(canvas->getDevice()->height());
    bounds.set_width(pixel_bounds.width() / device_scale_factor_);
    bounds.set_height(pixel_bounds.height() / device_scale_factor_);

    HISTOGRAM_COUNTS_100("MPArch.RW_PaintRectCount", update.paint_rects.size());

    pending_update_params_->bitmap = current_paint_buf_->id();
    pending_update_params_->bitmap_rect = bounds;

    std::vector<gfx::Rect>& copy_rects = pending_update_params_->copy_rects;
    // The scroll damage is just another rectangle to paint and copy.
    copy_rects.swap(update.paint_rects);
    if (!scroll_damage.IsEmpty())
      copy_rects.push_back(scroll_damage);

    for (size_t i = 0; i < copy_rects.size(); ++i) {
      gfx::Rect rect = copy_rects[i];
      if (fractional_scale) {
        // Damage might not be DPI aligned.  Inflate rect to compensate.
        rect.Inset(-1, -1);
      }
      PaintRect(rect, pixel_bounds.origin(), canvas.get());
    }

    // Software FPS tick for performance tests. The accelerated path traces the
    // frame events in didCommitAndDrawCompositorFrame. See
    // tab_capture_performancetest.cc.
    // NOTE: Tests may break if this event is renamed or moved.
    UNSHIPPED_TRACE_EVENT_INSTANT0("test_fps", "TestFrameTickSW",
                                   TRACE_EVENT_SCOPE_THREAD);
  } else {  // Accelerated compositing path
    // Begin painting.
    // If painting is done via the gpu process then we don't set any damage
    // rects to save the browser process from doing unecessary work.
    pending_update_params_->bitmap_rect = bounds;
    pending_update_params_->scroll_rect = gfx::Rect();
    // We don't need an ack, because we're not sharing a DIB with the browser.
    // If it needs to (e.g. composited UI), the GPU process does its own ACK
    // with the browser for the GPU surface.
    pending_update_params_->needs_ack = false;
    Composite(frame_begin_ticks);
  }

  // If we're holding a pending input event ACK, send the ACK before sending the
  // UpdateReply message so we can receive another input event before the
  // UpdateRect_ACK on platforms where the UpdateRect_ACK is sent from within
  // the UpdateRect IPC message handler.
  FlushPendingInputEventAck();

  // If Composite() called SwapBuffers, pending_update_params_ will be reset (in
  // OnSwapBuffersPosted), meaning a message has been added to the
  // updates_pending_swap_ queue, that will be sent later. Otherwise, we send
  // the message now.
  if (pending_update_params_) {
    // sending an ack to browser process that the paint is complete...
    update_reply_pending_ = pending_update_params_->needs_ack;
    Send(new ViewHostMsg_UpdateRect(routing_id_, *pending_update_params_));
    pending_update_params_.reset();
  }

  // If we're software rendering then we're done initiating the paint.
  if (!is_accelerated_compositing_active_)
    DidInitiatePaint();
}

void RenderWidget::Composite(base::TimeTicks frame_begin_time) {
  DCHECK(is_accelerated_compositing_active_);
  if (compositor_)  // TODO(jamesr): Figure out how this can be null.
    compositor_->Composite(frame_begin_time);
}

///////////////////////////////////////////////////////////////////////////////
// WebWidgetClient

void RenderWidget::didInvalidateRect(const WebRect& rect) {
  // The invalidated rect might be outside the bounds of the view.
  gfx::Rect view_rect(size_);
  gfx::Rect damaged_rect = gfx::IntersectRects(view_rect, rect);
  if (damaged_rect.IsEmpty())
    return;

  paint_aggregator_.InvalidateRect(damaged_rect);

  // We may not need to schedule another call to DoDeferredUpdate.
  if (invalidation_task_posted_)
    return;
  if (!paint_aggregator_.HasPendingUpdate())
    return;
  if (update_reply_pending_ ||
      num_swapbuffers_complete_pending_ >= kMaxSwapBuffersPending)
    return;

  // When GPU rendering, combine pending animations and invalidations into
  // a single update.
  if (is_accelerated_compositing_active_ &&
      animation_update_pending_ &&
      animation_timer_.IsRunning())
    return;

  // Perform updating asynchronously.  This serves two purposes:
  // 1) Ensures that we call WebView::Paint without a bunch of other junk
  //    on the call stack.
  // 2) Allows us to collect more damage rects before painting to help coalesce
  //    the work that we will need to do.
  invalidation_task_posted_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&RenderWidget::InvalidationCallback, this));
}

void RenderWidget::didScrollRect(int dx, int dy,
                                 const WebRect& clip_rect) {
  // Drop scrolls on the floor when we are in compositing mode.
  // TODO(nduca): stop WebViewImpl from sending scrolls in the first place.
  if (is_accelerated_compositing_active_)
    return;

  // The scrolled rect might be outside the bounds of the view.
  gfx::Rect view_rect(size_);
  gfx::Rect damaged_rect = gfx::IntersectRects(view_rect, clip_rect);
  if (damaged_rect.IsEmpty())
    return;

  paint_aggregator_.ScrollRect(gfx::Vector2d(dx, dy), damaged_rect);

  // We may not need to schedule another call to DoDeferredUpdate.
  if (invalidation_task_posted_)
    return;
  if (!paint_aggregator_.HasPendingUpdate())
    return;
  if (update_reply_pending_ ||
      num_swapbuffers_complete_pending_ >= kMaxSwapBuffersPending)
    return;

  // When GPU rendering, combine pending animations and invalidations into
  // a single update.
  if (is_accelerated_compositing_active_ &&
      animation_update_pending_ &&
      animation_timer_.IsRunning())
    return;

  // Perform updating asynchronously.  This serves two purposes:
  // 1) Ensures that we call WebView::Paint without a bunch of other junk
  //    on the call stack.
  // 2) Allows us to collect more damage rects before painting to help coalesce
  //    the work that we will need to do.
  invalidation_task_posted_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&RenderWidget::InvalidationCallback, this));
}

void RenderWidget::didAutoResize(const WebSize& new_size) {
  if (size_.width() != new_size.width || size_.height() != new_size.height) {
    size_ = new_size;

    // If we don't clear PaintAggregator after changing autoResize state, then
    // we might end up in a situation where bitmap_rect is larger than the
    // view_size. By clearing PaintAggregator, we ensure that we don't end up
    // with invalid damage rects.
    paint_aggregator_.ClearPendingUpdate();

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
  physical_backing_size_ = gfx::ToCeiledSize(gfx::ScaleSize(size_,
      device_scale_factor_));
  if (compositor_)
    compositor_->setViewportSize(size_, physical_backing_size_);
}

// FIXME: To be removed as soon as chromium and blink side changes land
// didActivateCompositor with parameters is still kept in order to land
// these changes s-chromium - https://codereview.chromium.org/137893025/.
// s-blink - https://codereview.chromium.org/138523003/
void RenderWidget::didActivateCompositor(int input_handler_identifier) {
  TRACE_EVENT0("gpu", "RenderWidget::didActivateCompositor");

#if !defined(OS_MACOSX)
  if (!is_accelerated_compositing_active_) {
    // When not in accelerated compositing mode, in certain cases (e.g. waiting
    // for a resize or if no backing store) the RenderWidgetHost is blocking the
    // browser's UI thread for some time, waiting for an UpdateRect. If we are
    // going to switch to accelerated compositing, the GPU process may need
    // round-trips to the browser's UI thread before finishing the frame,
    // causing deadlocks if we delay the UpdateRect until we receive the
    // OnSwapBuffersComplete.  So send a dummy message that will unblock the
    // browser's UI thread. This is not necessary on Mac, because SwapBuffers
    // now unblocks GetBackingStore on Mac.
    Send(new ViewHostMsg_UpdateIsDelayed(routing_id_));
  }
#endif

  is_accelerated_compositing_active_ = true;
  Send(new ViewHostMsg_DidActivateAcceleratedCompositing(
      routing_id_, is_accelerated_compositing_active_));

  if (!was_accelerated_compositing_ever_active_) {
    was_accelerated_compositing_ever_active_ = true;
    webwidget_->enterForceCompositingMode(true);
  }
}

void RenderWidget::didActivateCompositor() {
  TRACE_EVENT0("gpu", "RenderWidget::didActivateCompositor");

#if !defined(OS_MACOSX)
  if (!is_accelerated_compositing_active_) {
    // When not in accelerated compositing mode, in certain cases (e.g. waiting
    // for a resize or if no backing store) the RenderWidgetHost is blocking the
    // browser's UI thread for some time, waiting for an UpdateRect. If we are
    // going to switch to accelerated compositing, the GPU process may need
    // round-trips to the browser's UI thread before finishing the frame,
    // causing deadlocks if we delay the UpdateRect until we receive the
    // OnSwapBuffersComplete.  So send a dummy message that will unblock the
    // browser's UI thread. This is not necessary on Mac, because SwapBuffers
    // now unblocks GetBackingStore on Mac.
    Send(new ViewHostMsg_UpdateIsDelayed(routing_id_));
  }
#endif

  is_accelerated_compositing_active_ = true;
  Send(new ViewHostMsg_DidActivateAcceleratedCompositing(
      routing_id_, is_accelerated_compositing_active_));

  if (!was_accelerated_compositing_ever_active_) {
    was_accelerated_compositing_ever_active_ = true;
    webwidget_->enterForceCompositingMode(true);
  }
}

void RenderWidget::didDeactivateCompositor() {
  TRACE_EVENT0("gpu", "RenderWidget::didDeactivateCompositor");

  is_accelerated_compositing_active_ = false;
  Send(new ViewHostMsg_DidActivateAcceleratedCompositing(
      routing_id_, is_accelerated_compositing_active_));

  if (using_asynchronous_swapbuffers_)
    using_asynchronous_swapbuffers_ = false;

  // In single-threaded mode, we exit force compositing mode and re-enter in
  // DoDeferredUpdate() if appropriate. In threaded compositing mode,
  // DoDeferredUpdate() is bypassed and WebKit is responsible for exiting and
  // entering force compositing mode at the appropriate times.
  if (!is_threaded_compositing_enabled_)
    webwidget_->enterForceCompositingMode(false);
}

void RenderWidget::initializeLayerTreeView() {
  compositor_ = RenderWidgetCompositor::Create(
      this, is_threaded_compositing_enabled_);
  if (!compositor_)
    return;

  compositor_->setViewportSize(size_, physical_backing_size_);
  if (init_complete_)
    compositor_->setSurfaceReady();
}

blink::WebLayerTreeView* RenderWidget::layerTreeView() {
  return compositor_.get();
}

void RenderWidget::suppressCompositorScheduling(bool enable) {
  if (compositor_)
    compositor_->SetSuppressScheduleComposite(enable);
}

void RenderWidget::willBeginCompositorFrame() {
  TRACE_EVENT0("gpu", "RenderWidget::willBeginCompositorFrame");

  DCHECK(RenderThreadImpl::current()->compositor_message_loop_proxy().get());

  // The following two can result in further layout and possibly
  // enable GPU acceleration so they need to be called before any painting
  // is done.
  UpdateTextInputType();
#if defined(OS_ANDROID)
  UpdateTextInputState(false, true);
  UpdateSelectionRootBounds();
#endif
  UpdateSelectionBounds();
}

void RenderWidget::didBecomeReadyForAdditionalInput() {
  TRACE_EVENT0("renderer", "RenderWidget::didBecomeReadyForAdditionalInput");
  FlushPendingInputEventAck();
}

void RenderWidget::DidCommitCompositorFrame() {
  FOR_EACH_OBSERVER(RenderFrameImpl, swapped_out_frames_,
                    DidCommitCompositorFrame());
}

void RenderWidget::didCommitAndDrawCompositorFrame() {
  TRACE_EVENT0("gpu", "RenderWidget::didCommitAndDrawCompositorFrame");
  // Accelerated FPS tick for performance tests. See
  // tab_capture_performancetest.cc.  NOTE: Tests may break if this event is
  // renamed or moved.
  UNSHIPPED_TRACE_EVENT_INSTANT0("test_fps", "TestFrameTickGPU",
                                 TRACE_EVENT_SCOPE_THREAD);
  // Notify subclasses that we initiated the paint operation.
  DidInitiatePaint();
}

void RenderWidget::didCompleteSwapBuffers() {
  TRACE_EVENT0("renderer", "RenderWidget::didCompleteSwapBuffers");

  // Notify subclasses threaded composited rendering was flushed to the screen.
  DidFlushPaint();

  if (update_reply_pending_)
    return;

  if (!next_paint_flags_ &&
      !need_update_rect_for_auto_resize_ &&
      !plugin_window_moves_.size()) {
    return;
  }

  ViewHostMsg_UpdateRect_Params params;
  params.view_size = size_;
  params.plugin_window_moves.swap(plugin_window_moves_);
  params.flags = next_paint_flags_;
  params.scroll_offset = GetScrollOffset();
  params.needs_ack = false;
  params.scale_factor = device_scale_factor_;

  Send(new ViewHostMsg_UpdateRect(routing_id_, params));
  next_paint_flags_ = 0;
  need_update_rect_for_auto_resize_ = false;
}

void RenderWidget::scheduleComposite() {
  if (RenderThreadImpl::current()->compositor_message_loop_proxy().get() &&
      compositor_) {
      compositor_->setNeedsAnimate();
  } else {
    // TODO(nduca): replace with something a little less hacky.  The reason this
    // hack is still used is because the Invalidate-DoDeferredUpdate loop
    // contains a lot of host-renderer synchronization logic that is still
    // important for the accelerated compositing case. The option of simply
    // duplicating all that code is less desirable than "faking out" the
    // invalidation path using a magical damage rect.
    didInvalidateRect(WebRect(0, 0, 1, 1));
  }
}

void RenderWidget::scheduleAnimation() {
  if (animation_update_pending_)
    return;

  TRACE_EVENT0("gpu", "RenderWidget::scheduleAnimation");
  animation_update_pending_ = true;
  if (!animation_timer_.IsRunning()) {
    animation_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(0), this,
                           &RenderWidget::AnimationCallback);
  }
}

void RenderWidget::didChangeCursor(const WebCursorInfo& cursor_info) {
  // TODO(darin): Eliminate this temporary.
  WebCursor cursor;
  InitializeCursorFromWebKitCursorInfo(&cursor, cursor_info);
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
  // NOTE: initial_pos_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if as_popup is false, or the browser
  // process will impose a default position otherwise.
  Send(new ViewHostMsg_ShowWidget(opener_id_, routing_id_, initial_pos_));
  SetPendingWindowRect(initial_pos_);
}

void RenderWidget::didFocus() {
}

void RenderWidget::didBlur() {
}

void RenderWidget::DoDeferredClose() {
  Send(new ViewHostMsg_Close(routing_id_));
}

void RenderWidget::closeWidgetSoon() {
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
  base::MessageLoop::current()->PostTask(
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
  if (webwidget_) {
    webwidget_->willCloseLayerTreeView();
    compositor_.reset();
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
  WebRect pos = rect;
  if (popup_origin_scale_for_emulation_) {
    float scale = popup_origin_scale_for_emulation_;
    pos.x = popup_screen_origin_for_emulation_.x() +
        (pos.x - popup_view_origin_for_emulation_.x()) * scale;
    pos.y = popup_screen_origin_for_emulation_.y() +
        (pos.y - popup_view_origin_for_emulation_.y()) * scale;
  }

  if (!resizing_mode_selector_->is_synchronous_mode()) {
    if (did_show_) {
      Send(new ViewHostMsg_RequestMove(routing_id_, pos));
      SetPendingWindowRect(pos);
    } else {
      initial_pos_ = pos;
    }
  } else {
    ResizeSynchronously(pos);
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

void RenderWidget::OnSetInputMethodActive(bool is_active) {
  // To prevent this renderer process from sending unnecessary IPC messages to
  // a browser process, we permit the renderer process to send IPC messages
  // only during the input method attached to the browser process is active.
  input_method_is_active_ = is_active;
}

void RenderWidget::OnCandidateWindowShown() {
  webwidget_->didShowCandidateWindow();
}

void RenderWidget::OnCandidateWindowUpdated() {
  webwidget_->didUpdateCandidateWindow();
}

void RenderWidget::OnCandidateWindowHidden() {
  webwidget_->didHideCandidateWindow();
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
    Send(new ViewHostMsg_ImeCancelComposition(routing_id()));
  }
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(USE_AURA)
  UpdateCompositionInfo(true);
#endif
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
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(USE_AURA)
  UpdateCompositionInfo(true);
#endif
}

void RenderWidget::OnSnapshot(const gfx::Rect& src_subrect) {
  SkBitmap snapshot;

  if (OnSnapshotHelper(src_subrect, &snapshot)) {
    Send(new ViewHostMsg_Snapshot(routing_id(), true, snapshot));
  } else {
    Send(new ViewHostMsg_Snapshot(routing_id(), false, SkBitmap()));
  }
}

bool RenderWidget::OnSnapshotHelper(const gfx::Rect& src_subrect,
                                    SkBitmap* snapshot) {
  base::TimeTicks beginning_time = base::TimeTicks::Now();

  if (!webwidget_ || src_subrect.IsEmpty())
    return false;

  gfx::Rect viewport_size = gfx::IntersectRects(
      src_subrect, gfx::Rect(physical_backing_size_));

  skia::RefPtr<SkCanvas> canvas = skia::AdoptRef(
      skia::CreatePlatformCanvas(viewport_size.width(),
                                 viewport_size.height(),
                                 true,
                                 NULL,
                                 skia::RETURN_NULL_ON_FAILURE));
  if (!canvas)
    return false;

  canvas->save();
  webwidget_->layout();

  PaintRect(viewport_size, viewport_size.origin(), canvas.get());
  canvas->restore();

  const SkBitmap& bitmap = skia::GetTopDevice(*canvas)->accessBitmap(false);
  if (!bitmap.copyTo(snapshot, kPMColor_SkColorType))
    return false;

  UMA_HISTOGRAM_TIMES("Renderer4.Snapshot",
                      base::TimeTicks::Now() - beginning_time);
  return true;
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
  if (is_accelerated_compositing_active_ && compositor_) {
    compositor_->SetNeedsRedrawRect(gfx::Rect(size_to_paint));
  } else {
    gfx::Rect repaint_rect(size_to_paint.width(), size_to_paint.height());
    didInvalidateRect(repaint_rect);
  }
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

#if defined(OS_ANDROID)
void RenderWidget::OnShowImeIfNeeded() {
  UpdateTextInputState(true, true);
}

void RenderWidget::IncrementOutstandingImeEventAcks() {
  ++outstanding_ime_acks_;
}

void RenderWidget::OnImeEventAck() {
  --outstanding_ime_acks_;
  DCHECK(outstanding_ime_acks_ >= 0);
}
#endif

bool RenderWidget::ShouldHandleImeEvent() {
#if defined(OS_ANDROID)
  return !!webwidget_ && outstanding_ime_acks_ == 0;
#else
  return !!webwidget_;
#endif
}

void RenderWidget::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor_ == device_scale_factor)
    return;

  device_scale_factor_ = device_scale_factor;

  if (!is_accelerated_compositing_active_) {
    didInvalidateRect(gfx::Rect(size_.width(), size_.height()));
  } else {
    scheduleComposite();
  }
}

PepperPluginInstanceImpl* RenderWidget::GetBitmapForOptimizedPluginPaint(
    const gfx::Rect& paint_bounds,
    TransportDIB** dib,
    gfx::Rect* location,
    gfx::Rect* clip,
    float* scale_factor) {
  // Bare RenderWidgets don't support optimized plugin painting.
  return NULL;
}

gfx::Vector2d RenderWidget::GetScrollOffset() {
  // Bare RenderWidgets don't support scroll offset.
  return gfx::Vector2d();
}

void RenderWidget::SetHidden(bool hidden) {
  if (is_hidden_ == hidden)
    return;

  // The status has changed.  Tell the RenderThread about it.
  is_hidden_ = hidden;
  if (is_hidden_)
    RenderThreadImpl::current()->WidgetHidden();
  else
    RenderThreadImpl::current()->WidgetRestored();
}

void RenderWidget::WillToggleFullscreen() {
  if (!webwidget_)
    return;

  if (is_fullscreen_) {
    webwidget_->willExitFullScreen();
  } else {
    webwidget_->willEnterFullScreen();
  }
}

void RenderWidget::DidToggleFullscreen() {
  if (!webwidget_)
    return;

  if (is_fullscreen_) {
    webwidget_->didEnterFullScreen();
  } else {
    webwidget_->didExitFullScreen();
  }
}

void RenderWidget::SetBackground(const SkBitmap& background) {
  background_ = background;

  // Generate a full repaint.
  didInvalidateRect(gfx::Rect(size_.width(), size_.height()));
}

bool RenderWidget::next_paint_is_resize_ack() const {
  return ViewHostMsg_UpdateRect_Flags::is_resize_ack(next_paint_flags_);
}

bool RenderWidget::next_paint_is_restore_ack() const {
  return ViewHostMsg_UpdateRect_Flags::is_restore_ack(next_paint_flags_);
}

void RenderWidget::set_next_paint_is_resize_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK;
}

void RenderWidget::set_next_paint_is_restore_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_RESTORE_ACK;
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
#if defined(OS_ANDROID)
  UpdateSelectionRootBounds();
#endif
  UpdateSelectionBounds();
#if defined(OS_ANDROID)
  UpdateTextInputState(false, false);
#endif
}

void RenderWidget::UpdateTextInputType() {
  // On Windows, not only an IME but also an on-screen keyboard relies on the
  // latest TextInputType to optimize its layout and functionality. Thus
  // |input_method_is_active_| is no longer an appropriate condition to suppress
  // TextInputTypeChanged IPC on Windows.
  // TODO(yukawa, yoichio): Consider to stop checking |input_method_is_active_|
  // on other platforms as well as Windows if the overhead is acceptable.
#if !defined(OS_WIN)
  if (!input_method_is_active_)
    return;
#endif

  ui::TextInputType new_type = GetTextInputType();
  if (IsDateTimeInput(new_type))
    return;  // Not considered as a text input field in WebKit/Chromium.

  bool new_can_compose_inline = CanComposeInline();

  blink::WebTextInputInfo new_info;
  if (webwidget_)
    new_info = webwidget_->textInputInfo();
  const ui::TextInputMode new_mode = ConvertInputMode(new_info.inputMode);

  if (text_input_type_ != new_type
      || can_compose_inline_ != new_can_compose_inline
      || text_input_mode_ != new_mode) {
    Send(new ViewHostMsg_TextInputTypeChanged(routing_id(),
                                              new_type,
                                              new_mode,
                                              new_can_compose_inline));
    text_input_type_ = new_type;
    can_compose_inline_ = new_can_compose_inline;
    text_input_mode_ = new_mode;
  }
}

#if defined(OS_ANDROID) || defined(USE_AURA)
void RenderWidget::UpdateTextInputState(bool show_ime_if_needed,
                                        bool send_ime_ack) {
  if (handling_ime_event_)
    return;
  if (!show_ime_if_needed && !input_method_is_active_)
    return;
  ui::TextInputType new_type = GetTextInputType();
  if (IsDateTimeInput(new_type))
    return;  // Not considered as a text input field in WebKit/Chromium.

  blink::WebTextInputInfo new_info;
  if (webwidget_)
    new_info = webwidget_->textInputInfo();

  bool new_can_compose_inline = CanComposeInline();

  // Only sends text input params if they are changed or if the ime should be
  // shown.
  if (show_ime_if_needed || (text_input_type_ != new_type
      || text_input_info_ != new_info
      || can_compose_inline_ != new_can_compose_inline)) {
    ViewHostMsg_TextInputState_Params p;
#if defined(USE_AURA)
    p.require_ack = false;
#endif
    p.type = new_type;
    p.value = new_info.value.utf8();
    p.selection_start = new_info.selectionStart;
    p.selection_end = new_info.selectionEnd;
    p.composition_start = new_info.compositionStart;
    p.composition_end = new_info.compositionEnd;
    p.can_compose_inline = new_can_compose_inline;
    p.show_ime_if_needed = show_ime_if_needed;
#if defined(OS_ANDROID)
    p.require_ack = send_ime_ack;
    if (p.require_ack)
      IncrementOutstandingImeEventAcks();
#endif
    Send(new ViewHostMsg_TextInputStateChanged(routing_id(), p));

    text_input_info_ = new_info;
    text_input_type_ = new_type;
    can_compose_inline_ = new_can_compose_inline;
  }
}
#endif

void RenderWidget::GetSelectionBounds(gfx::Rect* focus, gfx::Rect* anchor) {
  WebRect focus_webrect;
  WebRect anchor_webrect;
  webwidget_->selectionBounds(focus_webrect, anchor_webrect);
  *focus = focus_webrect;
  *anchor = anchor_webrect;
}

void RenderWidget::UpdateSelectionBounds() {
  if (!webwidget_)
    return;
  if (handling_ime_event_)
    return;

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
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(USE_AURA)
  UpdateCompositionInfo(false);
#endif
}

// Check blink::WebTextInputType and ui::TextInputType is kept in sync.
COMPILE_ASSERT(int(blink::WebTextInputTypeNone) == \
               int(ui::TEXT_INPUT_TYPE_NONE), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeText) == \
               int(ui::TEXT_INPUT_TYPE_TEXT), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypePassword) == \
               int(ui::TEXT_INPUT_TYPE_PASSWORD), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeSearch) == \
               int(ui::TEXT_INPUT_TYPE_SEARCH), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeEmail) == \
               int(ui::TEXT_INPUT_TYPE_EMAIL), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeNumber) == \
               int(ui::TEXT_INPUT_TYPE_NUMBER), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeTelephone) == \
               int(ui::TEXT_INPUT_TYPE_TELEPHONE), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeURL) == \
               int(ui::TEXT_INPUT_TYPE_URL), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeDate) == \
               int(ui::TEXT_INPUT_TYPE_DATE), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeDateTime) == \
               int(ui::TEXT_INPUT_TYPE_DATE_TIME), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeDateTimeLocal) == \
               int(ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeMonth) == \
               int(ui::TEXT_INPUT_TYPE_MONTH), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeTime) == \
               int(ui::TEXT_INPUT_TYPE_TIME), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeWeek) == \
               int(ui::TEXT_INPUT_TYPE_WEEK), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeTextArea) == \
               int(ui::TEXT_INPUT_TYPE_TEXT_AREA), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeContentEditable) == \
               int(ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeDateTimeField) == \
               int(ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD), mismatching_enums);

ui::TextInputType RenderWidget::WebKitToUiTextInputType(
    blink::WebTextInputType type) {
  // Check the type is in the range representable by ui::TextInputType.
  DCHECK_LE(type, static_cast<int>(ui::TEXT_INPUT_TYPE_MAX)) <<
    "blink::WebTextInputType and ui::TextInputType not synchronized";
  return static_cast<ui::TextInputType>(type);
}

ui::TextInputType RenderWidget::GetTextInputType() {
  if (webwidget_)
    return WebKitToUiTextInputType(webwidget_->textInputInfo().type);
  return ui::TEXT_INPUT_TYPE_NONE;
}

#if defined(OS_MACOSX) || defined(OS_WIN) || defined(USE_AURA)
void RenderWidget::UpdateCompositionInfo(bool should_update_range) {
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
  Send(new ViewHostMsg_ImeCompositionRangeChanged(
      routing_id(), composition_range_, composition_character_bounds_));
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
  if (!input_method_is_active_)
    return;

  ImeEventGuard guard(this);
  // If the last text input type is not None, then we should finish any
  // ongoing composition regardless of the new text input type.
  if (text_input_type_ != ui::TEXT_INPUT_TYPE_NONE) {
    // If a composition text exists, then we need to let the browser process
    // to cancel the input method's ongoing composition session.
    if (webwidget_->confirmComposition())
      Send(new ViewHostMsg_ImeCancelComposition(routing_id()));
  }

#if defined(OS_MACOSX) || defined(OS_WIN) || defined(USE_AURA)
  UpdateCompositionInfo(true);
#endif
}

void RenderWidget::didHandleGestureEvent(
    const WebGestureEvent& event,
    bool event_cancelled) {
#if defined(OS_ANDROID) || defined(USE_AURA)
  if (event_cancelled)
    return;
  if (event.type == WebInputEvent::GestureTap ||
      event.type == WebInputEvent::GestureLongPress) {
    UpdateTextInputState(true, true);
  }
#endif
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

void RenderWidget::hasTouchEventHandlers(bool has_handlers) {
  Send(new ViewHostMsg_HasTouchEventHandlers(routing_id_, has_handlers));
}

void RenderWidget::setTouchAction(
    blink::WebTouchAction web_touch_action) {

  // Ignore setTouchAction calls that result from synthetic touch events (eg.
  // when blink is emulating touch with mouse).
  if (!handling_touchstart_event_)
    return;

   // Verify the same values are used by the types so we can cast between them.
   COMPILE_ASSERT(static_cast<blink::WebTouchAction>(TOUCH_ACTION_AUTO) ==
                      blink::WebTouchActionAuto,
                  enum_values_must_match_for_touch_action);
   COMPILE_ASSERT(static_cast<blink::WebTouchAction>(TOUCH_ACTION_NONE) ==
                      blink::WebTouchActionNone,
                  enum_values_must_match_for_touch_action);
   COMPILE_ASSERT(static_cast<blink::WebTouchAction>(TOUCH_ACTION_PAN_X) ==
                      blink::WebTouchActionPanX,
                  enum_values_must_match_for_touch_action);
   COMPILE_ASSERT(static_cast<blink::WebTouchAction>(TOUCH_ACTION_PAN_Y) ==
                      blink::WebTouchActionPanY,
                  enum_values_must_match_for_touch_action);
   COMPILE_ASSERT(
       static_cast<blink::WebTouchAction>(TOUCH_ACTION_PINCH_ZOOM) ==
           blink::WebTouchActionPinchZoom,
       enum_values_must_match_for_touch_action);

   content::TouchAction content_touch_action =
       static_cast<content::TouchAction>(web_touch_action);
  Send(new InputHostMsg_SetTouchAction(routing_id_, content_touch_action));
}

#if defined(OS_ANDROID)
void RenderWidget::UpdateSelectionRootBounds() {
}
#endif

bool RenderWidget::HasTouchEventHandlersAt(const gfx::Point& point) const {
  return true;
}

scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
RenderWidget::CreateGraphicsContext3D() {
  if (!webwidget_)
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuCompositing))
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  if (!RenderThreadImpl::current())
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  CauseForGpuLaunch cause =
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;
  scoped_refptr<GpuChannelHost> gpu_channel_host(
      RenderThreadImpl::current()->EstablishGpuChannelSync(cause));
  if (!gpu_channel_host)
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
  // If we raster too fast we become upload bound, and pending
  // uploads consume memory. For maximum upload throughput, we would
  // want to allow for upload_throughput * pipeline_time of pending
  // uploads, after which we are just wasting memory. Since we don't
  // know our upload throughput yet, this just caps our memory usage.
  size_t divider = 1;
  if (base::android::SysUtils::IsLowEndDevice())
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

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
      new WebGraphicsContext3DCommandBufferImpl(surface_id(),
                                                GetURLForGraphicsContext3D(),
                                                gpu_channel_host.get(),
                                                attributes,
                                                lose_context_when_out_of_memory,
                                                limits,
                                                NULL));
  return context.Pass();
}

void RenderWidget::RegisterSwappedOutChildFrame(RenderFrameImpl* frame) {
  swapped_out_frames_.AddObserver(frame);
}

void RenderWidget::UnregisterSwappedOutChildFrame(RenderFrameImpl* frame) {
  swapped_out_frames_.RemoveObserver(frame);
}

}  // namespace content
