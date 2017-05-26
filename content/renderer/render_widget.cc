// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_synthetic_delay.h"
#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/input/touch_action.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/output/copy_output_request.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/trees/layer_tree_host.h"
#include "content/common/content_switches_internal.h"
#include "content/common/drag_event_source_info.h"
#include "content/common/drag_messages.h"
#include "content/common/input/synthetic_gesture_packet.h"
#include "content/common/input_messages.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/text_input_state.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/drop_data.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/cursor_utils.h"
#include "content/renderer/devtools/render_widget_screen_metrics_emulator.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/external_popup_menu.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "content/renderer/gpu/queue_message_swap_promise.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/input/main_thread_event_queue.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget_owner_delegate.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/resizing_mode_selector.h"
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_sync_message.h"
#include "ppapi/features/features.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/FilePathConversion.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/platform/WebDragData.h"
#include "third_party/WebKit/public/platform/WebDragOperation.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/render_widget_scheduling_state.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/renderer_scheduler.h"
#include "third_party/WebKit/public/web/WebAutofillClient.h"
#include "third_party/WebKit/public/web/WebDeviceEmulationParams.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebInputMethodController.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebPopupMenuInfo.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_switches.h"
#include "ui/surface/transport_dib.h"

#if defined(OS_ANDROID)
#include <android/keycodes.h>
#include "base/time/time.h"
#endif

#if defined(OS_POSIX)
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#endif  // defined(OS_POSIX)

#if defined(USE_AURA)
#include "content/renderer/mus/renderer_window_tree_client.h"
#endif

#if defined(OS_MACOSX)
#include "content/renderer/text_input_client_observer.h"
#endif

using blink::WebCompositionUnderline;
using blink::WebCursorInfo;
using blink::WebDeviceEmulationParams;
using blink::WebDragOperation;
using blink::WebDragOperationsMask;
using blink::WebDragData;
using blink::WebFrameWidget;
using blink::WebGestureEvent;
using blink::WebImage;
using blink::WebInputEvent;
using blink::WebInputEventResult;
using blink::WebInputMethodController;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebNavigationPolicy;
using blink::WebNode;
using blink::WebPagePopup;
using blink::WebPoint;
using blink::WebPopupType;
using blink::WebRange;
using blink::WebRect;
using blink::WebSize;
using blink::WebString;
using blink::WebTextDirection;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using blink::WebVector;
using blink::WebWidget;

namespace content {

namespace {

typedef std::map<std::string, ui::TextInputMode> TextInputModeMap;

class WebWidgetLockTarget : public content::MouseLockDispatcher::LockTarget {
 public:
  explicit WebWidgetLockTarget(blink::WebWidget* webwidget)
      : webwidget_(webwidget) {}

  void OnLockMouseACK(bool succeeded) override {
    if (succeeded)
      webwidget_->DidAcquirePointerLock();
    else
      webwidget_->DidNotAcquirePointerLock();
  }

  void OnMouseLockLost() override { webwidget_->DidLosePointerLock(); }

  bool HandleMouseLockedInputEvent(const blink::WebMouseEvent& event) override {
    // The WebWidget handles mouse lock in Blink's handleInputEvent().
    return false;
  }

 private:
  blink::WebWidget* webwidget_;
};

bool IsDateTimeInput(ui::TextInputType type) {
  return type == ui::TEXT_INPUT_TYPE_DATE ||
         type == ui::TEXT_INPUT_TYPE_DATE_TIME ||
         type == ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL ||
         type == ui::TEXT_INPUT_TYPE_MONTH ||
         type == ui::TEXT_INPUT_TYPE_TIME || type == ui::TEXT_INPUT_TYPE_WEEK;
}

WebDragData DropMetaDataToWebDragData(
    const std::vector<DropData::Metadata>& drop_meta_data) {
  std::vector<WebDragData::Item> item_list;
  for (const auto& meta_data_item : drop_meta_data) {
    if (meta_data_item.kind == DropData::Kind::STRING) {
      WebDragData::Item item;
      item.storage_type = WebDragData::Item::kStorageTypeString;
      item.string_type = WebString::FromUTF16(meta_data_item.mime_type);
      // Have to pass a dummy URL here instead of an empty URL because the
      // DropData received by browser_plugins goes through a round trip:
      // DropData::MetaData --> WebDragData-->DropData. In the end, DropData
      // will contain an empty URL (which means no URL is dragged) if the URL in
      // WebDragData is empty.
      if (base::EqualsASCII(meta_data_item.mime_type,
                            ui::Clipboard::kMimeTypeURIList)) {
        item.string_data = WebString::FromUTF8("about:dragdrop-placeholder");
      }
      item_list.push_back(item);
      continue;
    }

    // TODO(hush): crbug.com/584789. Blink needs to support creating a file with
    // just the mimetype. This is needed to drag files to WebView on Android
    // platform.
    if ((meta_data_item.kind == DropData::Kind::FILENAME) &&
        !meta_data_item.filename.empty()) {
      WebDragData::Item item;
      item.storage_type = WebDragData::Item::kStorageTypeFilename;
      item.filename_data = blink::FilePathToWebString(meta_data_item.filename);
      item_list.push_back(item);
      continue;
    }

    if (meta_data_item.kind == DropData::Kind::FILESYSTEMFILE) {
      WebDragData::Item item;
      item.storage_type = WebDragData::Item::kStorageTypeFileSystemFile;
      item.file_system_url = meta_data_item.file_system_url;
      item_list.push_back(item);
      continue;
    }
  }

  WebDragData result;
  result.Initialize();
  result.SetItems(item_list);
  return result;
}

WebDragData DropDataToWebDragData(const DropData& drop_data) {
  std::vector<WebDragData::Item> item_list;

  // These fields are currently unused when dragging into WebKit.
  DCHECK(drop_data.download_metadata.empty());
  DCHECK(drop_data.file_contents.empty());
  DCHECK(drop_data.file_contents_content_disposition.empty());

  if (!drop_data.text.is_null()) {
    WebDragData::Item item;
    item.storage_type = WebDragData::Item::kStorageTypeString;
    item.string_type = WebString::FromUTF8(ui::Clipboard::kMimeTypeText);
    item.string_data = WebString::FromUTF16(drop_data.text.string());
    item_list.push_back(item);
  }

  if (!drop_data.url.is_empty()) {
    WebDragData::Item item;
    item.storage_type = WebDragData::Item::kStorageTypeString;
    item.string_type = WebString::FromUTF8(ui::Clipboard::kMimeTypeURIList);
    item.string_data = WebString::FromUTF8(drop_data.url.spec());
    item.title = WebString::FromUTF16(drop_data.url_title);
    item_list.push_back(item);
  }

  if (!drop_data.html.is_null()) {
    WebDragData::Item item;
    item.storage_type = WebDragData::Item::kStorageTypeString;
    item.string_type = WebString::FromUTF8(ui::Clipboard::kMimeTypeHTML);
    item.string_data = WebString::FromUTF16(drop_data.html.string());
    item.base_url = drop_data.html_base_url;
    item_list.push_back(item);
  }

  for (std::vector<ui::FileInfo>::const_iterator it =
           drop_data.filenames.begin();
       it != drop_data.filenames.end();
       ++it) {
    WebDragData::Item item;
    item.storage_type = WebDragData::Item::kStorageTypeFilename;
    item.filename_data = blink::FilePathToWebString(it->path);
    item.display_name_data =
        blink::FilePathToWebString(base::FilePath(it->display_name));
    item_list.push_back(item);
  }

  for (std::vector<DropData::FileSystemFileInfo>::const_iterator it =
           drop_data.file_system_files.begin();
       it != drop_data.file_system_files.end();
       ++it) {
    WebDragData::Item item;
    item.storage_type = WebDragData::Item::kStorageTypeFileSystemFile;
    item.file_system_url = it->url;
    item.file_system_file_size = it->size;
    item.file_system_id = blink::WebString::FromASCII(it->filesystem_id);
    item_list.push_back(item);
  }

  for (std::map<base::string16, base::string16>::const_iterator it =
           drop_data.custom_data.begin();
       it != drop_data.custom_data.end();
       ++it) {
    WebDragData::Item item;
    item.storage_type = WebDragData::Item::kStorageTypeString;
    item.string_type = WebString::FromUTF16(it->first);
    item.string_data = WebString::FromUTF16(it->second);
    item_list.push_back(item);
  }

  WebDragData result;
  result.Initialize();
  result.SetItems(item_list);
  result.SetFilesystemId(WebString::FromUTF16(drop_data.filesystem_id));
  return result;
}

content::RenderWidget::CreateRenderWidgetFunction g_create_render_widget =
    nullptr;

content::RenderWidget::RenderWidgetInitializedCallback
    g_render_widget_initialized = nullptr;

ui::TextInputType ConvertWebTextInputType(blink::WebTextInputType type) {
  // Check the type is in the range representable by ui::TextInputType.
  DCHECK_LE(type, static_cast<int>(ui::TEXT_INPUT_TYPE_MAX))
      << "blink::WebTextInputType and ui::TextInputType not synchronized";
  return static_cast<ui::TextInputType>(type);
}

ui::TextInputMode ConvertWebTextInputMode(blink::WebTextInputMode mode) {
  // Check the mode is in the range representable by ui::TextInputMode.
  DCHECK_LE(mode, static_cast<int>(ui::TEXT_INPUT_MODE_MAX))
      << "blink::WebTextInputMode and ui::TextInputMode not synchronized";
  return static_cast<ui::TextInputMode>(mode);
}

}  // namespace

// RenderWidget ---------------------------------------------------------------

RenderWidget::RenderWidget(int32_t widget_routing_id,
                           CompositorDependencies* compositor_deps,
                           blink::WebPopupType popup_type,
                           const ScreenInfo& screen_info,
                           bool swapped_out,
                           bool hidden,
                           bool never_visible)
    : routing_id_(widget_routing_id),
      compositor_deps_(compositor_deps),
      webwidget_internal_(nullptr),
      owner_delegate_(nullptr),
      next_paint_flags_(0),
      auto_resize_mode_(false),
      need_update_rect_for_auto_resize_(false),
      did_show_(false),
      is_hidden_(hidden),
      compositor_never_visible_(never_visible),
      is_fullscreen_granted_(false),
      display_mode_(blink::kWebDisplayModeUndefined),
      ime_event_guard_(nullptr),
      closing_(false),
      host_closing_(false),
      is_swapped_out_(swapped_out),
      for_oopif_(false),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      text_input_mode_(ui::TEXT_INPUT_MODE_DEFAULT),
      text_input_flags_(0),
      can_compose_inline_(true),
      composition_range_(gfx::Range::InvalidRange()),
      popup_type_(popup_type),
      pending_window_rect_count_(0),
      screen_info_(screen_info),
      device_scale_factor_(screen_info_.device_scale_factor),
      monitor_composition_info_(false),
      popup_origin_scale_for_emulation_(0.f),
      frame_swap_message_queue_(new FrameSwapMessageQueue()),
      resizing_mode_selector_(new ResizingModeSelector()),
      has_host_context_menu_location_(false),
      has_added_input_handler_(false),
      has_focus_(false),
#if defined(OS_MACOSX)
      text_input_client_observer_(new TextInputClientObserver(this)),
#endif
      focused_pepper_plugin_(nullptr),
      time_to_first_active_paint_recorded_(true),
      was_shown_time_(base::TimeTicks::Now()),
      current_content_source_id_(0),
      weak_ptr_factory_(this) {
  DCHECK_NE(routing_id_, MSG_ROUTING_NONE);
  if (!swapped_out)
    RenderProcess::current()->AddRefProcess();
  DCHECK(RenderThread::Get());

  // In tests there may not be a RenderThreadImpl.
  if (RenderThreadImpl::current()) {
    render_widget_scheduling_state_ = RenderThreadImpl::current()
                                          ->GetRendererScheduler()
                                          ->NewRenderWidgetSchedulingState();
    render_widget_scheduling_state_->SetHidden(is_hidden_);
  }
#if defined(USE_AURA)
  RendererWindowTreeClient::CreateIfNecessary(routing_id_);
#endif
}

RenderWidget::~RenderWidget() {
  DCHECK(!webwidget_internal_) << "Leaking our WebWidget!";

  if (input_event_queue_)
    input_event_queue_->ClearClient();

  // If we are swapped out, we have released already.
  if (!is_swapped_out_ && RenderProcess::current())
    RenderProcess::current()->ReleaseProcess();
#if defined(USE_AURA)
  // It is possible for a RenderWidget to be destroyed before it was embedded
  // in a mus window. The RendererWindowTreeClient will leak in such cases. So
  // explicitly delete it here.
  RendererWindowTreeClient::Destroy(routing_id_);
#endif
}

// static
void RenderWidget::InstallCreateHook(
    CreateRenderWidgetFunction create_render_widget,
    RenderWidgetInitializedCallback render_widget_initialized) {
  CHECK(!g_create_render_widget && !g_render_widget_initialized);
  g_create_render_widget = create_render_widget;
  g_render_widget_initialized = render_widget_initialized;
}

// static
RenderWidget* RenderWidget::CreateForPopup(
    RenderViewImpl* opener,
    CompositorDependencies* compositor_deps,
    blink::WebPopupType popup_type,
    const ScreenInfo& screen_info) {
  // Do a synchronous IPC to obtain a routing ID.
  int32_t routing_id = MSG_ROUTING_NONE;
  if (!RenderThreadImpl::current_render_message_filter()->CreateNewWidget(
          opener->GetRoutingID(), popup_type, &routing_id)) {
    return nullptr;
  }

  scoped_refptr<RenderWidget> widget(
      new RenderWidget(routing_id, compositor_deps, popup_type, screen_info,
                       false, false, false));
  ShowCallback opener_callback =
      base::Bind(&RenderViewImpl::ShowCreatedPopupWidget, opener->GetWeakPtr());
  widget->Init(opener_callback, RenderWidget::CreateWebWidget(widget.get()));
  DCHECK(!widget->HasOneRef());  // RenderWidget::Init() adds a reference.
  return widget.get();
}

// static
RenderWidget* RenderWidget::CreateForFrame(
    int widget_routing_id,
    bool hidden,
    const ScreenInfo& screen_info,
    CompositorDependencies* compositor_deps,
    blink::WebLocalFrame* frame) {
  CHECK_NE(widget_routing_id, MSG_ROUTING_NONE);
  // TODO(avi): Before RenderViewImpl has-a RenderWidget, the browser passes the
  // same routing ID for both the view routing ID and the main frame widget
  // routing ID. https://crbug.com/545684
  RenderViewImpl* view = RenderViewImpl::FromRoutingID(widget_routing_id);
  if (view) {
    view->AttachWebFrameWidget(
        RenderWidget::CreateWebFrameWidget(view->GetWidget(), frame));
    return view->GetWidget();
  }
  scoped_refptr<RenderWidget> widget(
      g_create_render_widget
          ? g_create_render_widget(widget_routing_id, compositor_deps,
                                   blink::kWebPopupTypeNone, screen_info, false,
                                   hidden, false)
          : new RenderWidget(widget_routing_id, compositor_deps,
                             blink::kWebPopupTypeNone, screen_info, false,
                             hidden, false));
  widget->for_oopif_ = true;
  // Init increments the reference count on |widget|, keeping it alive after
  // this function returns.
  widget->Init(RenderWidget::ShowCallback(),
               RenderWidget::CreateWebFrameWidget(widget.get(), frame));

  if (g_render_widget_initialized)
    g_render_widget_initialized(widget.get());
  return widget.get();
}

// static
blink::WebFrameWidget* RenderWidget::CreateWebFrameWidget(
    RenderWidget* render_widget,
    blink::WebLocalFrame* frame) {
  if (!frame->Parent()) {
    // TODO(dcheng): The main frame widget currently has a special case.
    // Eliminate this once WebView is no longer a WebWidget.
    return blink::WebFrameWidget::Create(render_widget, frame->View(), frame);
  }
  return blink::WebFrameWidget::Create(render_widget, frame);
}

// static
blink::WebWidget* RenderWidget::CreateWebWidget(RenderWidget* render_widget) {
  switch (render_widget->popup_type_) {
    case blink::kWebPopupTypeNone:  // Nothing to create.
      break;
    case blink::kWebPopupTypePage:
      return WebPagePopup::Create(render_widget);
    default:
      NOTREACHED();
  }
  return NULL;
}

void RenderWidget::CloseForFrame() {
  OnClose();
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

void RenderWidget::Init(const ShowCallback& show_callback,
                        WebWidget* web_widget) {
  DCHECK(!webwidget_internal_);
  DCHECK_NE(routing_id_, MSG_ROUTING_NONE);

  input_handler_ = base::MakeUnique<RenderWidgetInputHandler>(this, this);

  show_callback_ = show_callback;

  webwidget_internal_ = web_widget;
  webwidget_mouse_lock_target_.reset(
      new WebWidgetLockTarget(webwidget_internal_));
  mouse_lock_dispatcher_.reset(new RenderWidgetMouseLockDispatcher(this));

  RenderThread::Get()->AddRoute(routing_id_, this);
  // Take a reference on behalf of the RenderThread.  This will be balanced
  // when we receive ViewMsg_Close.
  AddRef();
  if (RenderThreadImpl::current()) {
    RenderThreadImpl::current()->WidgetCreated();
    if (is_hidden_)
      RenderThreadImpl::current()->WidgetHidden();
  }
}

void RenderWidget::WasSwappedOut() {
  // If we have been swapped out and no one else is using this process,
  // it's safe to exit now.
  CHECK(is_swapped_out_);
  RenderProcess::current()->ReleaseProcess();
}

void RenderWidget::SetPopupOriginAdjustmentsForEmulation(
    RenderWidgetScreenMetricsEmulator* emulator) {
  popup_origin_scale_for_emulation_ = emulator->scale();
  popup_view_origin_for_emulation_ = emulator->applied_widget_rect().origin();
  popup_screen_origin_for_emulation_ = gfx::Point(
      emulator->original_screen_rect().origin().x() + emulator->offset().x(),
      emulator->original_screen_rect().origin().y() + emulator->offset().y());
  screen_info_ = emulator->original_screen_info();
  device_scale_factor_ = screen_info_.device_scale_factor;
}

gfx::Rect RenderWidget::AdjustValidationMessageAnchor(const gfx::Rect& anchor) {
  if (screen_metrics_emulator_)
    return screen_metrics_emulator_->AdjustValidationMessageAnchor(anchor);
  return anchor;
}

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
void RenderWidget::SetExternalPopupOriginAdjustmentsForEmulation(
    ExternalPopupMenu* popup,
    RenderWidgetScreenMetricsEmulator* emulator) {
  popup->SetOriginScaleAndOffsetForEmulation(
      emulator->scale(), emulator->offset());
}
#endif

void RenderWidget::OnShowHostContextMenu(ContextMenuParams* params) {
  if (screen_metrics_emulator_)
    screen_metrics_emulator_->OnShowContextMenu(params);
}

bool RenderWidget::OnMessageReceived(const IPC::Message& message) {
#if defined(OS_MACOSX)
  if (IPC_MESSAGE_CLASS(message) == TextInputClientMsgStart)
    return text_input_client_observer_->OnMessageReceived(message);
#endif
  if (mouse_lock_dispatcher_ &&
      mouse_lock_dispatcher_->OnMessageReceived(message))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidget, message)
    IPC_MESSAGE_HANDLER(InputMsg_HandleInputEvent, OnHandleInputEvent)
    IPC_MESSAGE_HANDLER(InputMsg_CursorVisibilityChange,
                        OnCursorVisibilityChange)
    IPC_MESSAGE_HANDLER(InputMsg_ImeSetComposition, OnImeSetComposition)
    IPC_MESSAGE_HANDLER(InputMsg_ImeCommitText, OnImeCommitText)
    IPC_MESSAGE_HANDLER(InputMsg_ImeFinishComposingText,
                        OnImeFinishComposingText)
    IPC_MESSAGE_HANDLER(InputMsg_MouseCaptureLost, OnMouseCaptureLost)
    IPC_MESSAGE_HANDLER(InputMsg_SetEditCommandsForNextKeyEvent,
                        OnSetEditCommandsForNextKeyEvent)
    IPC_MESSAGE_HANDLER(InputMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(InputMsg_SyntheticGestureCompleted,
                        OnSyntheticGestureCompleted)
    IPC_MESSAGE_HANDLER(ViewMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(ViewMsg_Resize, OnResize)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableDeviceEmulation,
                        OnEnableDeviceEmulation)
    IPC_MESSAGE_HANDLER(ViewMsg_DisableDeviceEmulation,
                        OnDisableDeviceEmulation)
    IPC_MESSAGE_HANDLER(ViewMsg_WasHidden, OnWasHidden)
    IPC_MESSAGE_HANDLER(ViewMsg_WasShown, OnWasShown)
    IPC_MESSAGE_HANDLER(ViewMsg_Repaint, OnRepaint)
    IPC_MESSAGE_HANDLER(ViewMsg_SetTextDirection, OnSetTextDirection)
    IPC_MESSAGE_HANDLER(ViewMsg_Move_ACK, OnRequestMoveAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateScreenRects, OnUpdateScreenRects)
    IPC_MESSAGE_HANDLER(ViewMsg_SetViewportIntersection,
                        OnSetViewportIntersection)
    IPC_MESSAGE_HANDLER(ViewMsg_WaitForNextFrameForTests,
                        OnWaitNextFrameForTests)
    IPC_MESSAGE_HANDLER(InputMsg_RequestCompositionUpdates,
                        OnRequestCompositionUpdates)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragEnter, OnDragTargetDragEnter)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragOver, OnDragTargetDragOver)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragLeave, OnDragTargetDragLeave)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDrop, OnDragTargetDrop)
    IPC_MESSAGE_HANDLER(DragMsg_SourceEnded, OnDragSourceEnded)
    IPC_MESSAGE_HANDLER(DragMsg_SourceSystemDragEnded,
                        OnDragSourceSystemDragEnded)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(InputMsg_RequestTextInputStateUpdate,
                        OnRequestTextInputStateUpdate)
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

void RenderWidget::SendOrCrash(IPC::Message* message) {
  bool result = Send(message);
  CHECK(closing_ || result) << "Failed to send message";
}

bool RenderWidget::ShouldHandleImeEvents() const {
  // TODO(ekaramad): We track page focus in all RenderViews on the page but the
  // RenderWidgets corresponding to OOPIFs do not get the update. For now, this
  // method returns true when the RenderWidget is for an OOPIF, i.e., IME events
  // will be processed regardless of page focus. We should revisit this after
  // page focus for OOPIFs has been fully resolved (https://crbug.com/689777).
  return GetWebWidget() && GetWebWidget()->IsWebFrameWidget() &&
         (has_focus_ || for_oopif_);
}

void RenderWidget::SetWindowRectSynchronously(
    const gfx::Rect& new_window_rect) {
  ResizeParams params;
  params.screen_info = screen_info_;
  params.new_size = new_window_rect.size();
  params.physical_backing_size =
      gfx::ScaleToCeiledSize(new_window_rect.size(), device_scale_factor_);
  params.visible_viewport_size = new_window_rect.size();
  params.is_fullscreen_granted = is_fullscreen_granted_;
  params.display_mode = display_mode_;
  params.needs_resize_ack = false;
  Resize(params);

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

void RenderWidget::OnResize(const ResizeParams& params) {
  if (resizing_mode_selector_->ShouldAbortOnResize(this, params))
    return;

  if (screen_metrics_emulator_) {
    screen_metrics_emulator_->OnResize(params);
    return;
  }

  Resize(params);
}

void RenderWidget::OnEnableDeviceEmulation(
   const blink::WebDeviceEmulationParams& params) {
  if (!screen_metrics_emulator_) {
    ResizeParams resize_params;
    resize_params.screen_info = screen_info_;
    resize_params.new_size = size_;
    resize_params.physical_backing_size = physical_backing_size_;
    resize_params.visible_viewport_size = visible_viewport_size_;
    resize_params.is_fullscreen_granted = is_fullscreen_granted_;
    resize_params.display_mode = display_mode_;
    screen_metrics_emulator_.reset(new RenderWidgetScreenMetricsEmulator(
        this, params, resize_params, view_screen_rect_, window_screen_rect_));
    screen_metrics_emulator_->Apply();
  } else {
    screen_metrics_emulator_->ChangeEmulationParams(params);
  }
}

void RenderWidget::OnDisableDeviceEmulation() {
  screen_metrics_emulator_.reset();
}

void RenderWidget::OnWasHidden() {
  TRACE_EVENT0("renderer", "RenderWidget::OnWasHidden");
  // Go into a mode where we stop generating paint and scrolling events.
  SetHidden(true);
  for (auto& observer : render_frames_)
    observer.WasHidden();
}

void RenderWidget::OnWasShown(bool needs_repainting,
                              const ui::LatencyInfo& latency_info) {
  TRACE_EVENT0("renderer", "RenderWidget::OnWasShown");
  // During shutdown we can just ignore this message.
  if (!GetWebWidget())
    return;

  was_shown_time_ = base::TimeTicks::Now();
  // See OnWasHidden
  SetHidden(false);
  for (auto& observer : render_frames_)
    observer.WasShown();

  if (!needs_repainting)
    return;

  if (compositor_) {
    ui::LatencyInfo swap_latency_info(latency_info);
    std::unique_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor =
        compositor_->CreateLatencyInfoSwapPromiseMonitor(&swap_latency_info);
    // Force this SwapPromiseMonitor to trigger and insert a SwapPromise.
    compositor_->SetNeedsBeginFrame();
  }
}

void RenderWidget::OnRequestMoveAck() {
  DCHECK(pending_window_rect_count_);
  pending_window_rect_count_--;
}

GURL RenderWidget::GetURLForGraphicsContext3D() {
  return GURL();
}

void RenderWidget::OnHandleInputEvent(
    const blink::WebInputEvent* input_event,
    const std::vector<const blink::WebInputEvent*>& coalesced_events,
    const ui::LatencyInfo& latency_info,
    InputEventDispatchType dispatch_type) {
  if (!input_event)
    return;

  input_handler_->HandleInputEvent(
      blink::WebCoalescedInputEvent(*input_event, coalesced_events),
      latency_info, dispatch_type);
}

InputEventAckState RenderWidget::HandleInputEvent(
    const blink::WebCoalescedInputEvent& input_event,
    const ui::LatencyInfo& latency_info,
    InputEventDispatchType dispatch_type) {
  return input_handler_->HandleInputEvent(input_event, latency_info,
                                          dispatch_type);
}

void RenderWidget::SendInputEventAck(blink::WebInputEvent::Type type,
                                     InputEventAckState ack_result,
                                     uint32_t touch_event_id) {
  InputEventAck ack(InputEventAckSource::MAIN_THREAD, type, ack_result,
                    touch_event_id);
  Send(new InputHostMsg_HandleInputEvent_ACK(routing_id_, ack));
}

void RenderWidget::OnCursorVisibilityChange(bool is_visible) {
  if (GetWebWidget())
    GetWebWidget()->SetCursorVisibilityState(is_visible);
}

void RenderWidget::OnMouseCaptureLost() {
  if (GetWebWidget())
    GetWebWidget()->MouseCaptureLost();
}

void RenderWidget::OnSetEditCommandsForNextKeyEvent(
    const EditCommands& edit_commands) {
  edit_commands_ = edit_commands;
}

void RenderWidget::OnSetFocus(bool enable) {
  has_focus_ = enable;

  if (GetWebWidget())
    GetWebWidget()->SetFocus(enable);

  for (auto& observer : render_frames_)
    observer.RenderWidgetSetFocus(enable);
}

void RenderWidget::SetNeedsMainFrame() {
  RenderWidgetCompositor* rwc = compositor();
  if (!rwc)
    return;
  rwc->SetNeedsBeginFrame();
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetCompositorDelegate

void RenderWidget::ApplyViewportDeltas(
    const gfx::Vector2dF& inner_delta,
    const gfx::Vector2dF& outer_delta,
    const gfx::Vector2dF& elastic_overscroll_delta,
    float page_scale,
    float top_controls_delta) {
  GetWebWidget()->ApplyViewportDeltas(inner_delta, outer_delta,
                                      elastic_overscroll_delta, page_scale,
                                      top_controls_delta);
}

void RenderWidget::RecordWheelAndTouchScrollingCount(
    bool has_scrolled_by_wheel,
    bool has_scrolled_by_touch) {
  GetWebWidget()->RecordWheelAndTouchScrollingCount(has_scrolled_by_wheel,
                                                    has_scrolled_by_touch);
}

void RenderWidget::BeginMainFrame(double frame_time_sec) {
  if (input_event_queue_) {
    input_event_queue_->DispatchRafAlignedInput(
        ui::EventTimeStampFromSeconds(frame_time_sec));
  }

  GetWebWidget()->BeginFrame(frame_time_sec);
}

void RenderWidget::RequestNewCompositorFrameSink(
    bool fallback,
    const CompositorFrameSinkCallback& callback) {
  DCHECK(GetWebWidget());
  // For widgets that are never visible, we don't start the compositor, so we
  // never get a request for a cc::CompositorFrameSink.
  DCHECK(!compositor_never_visible_);
  RenderThreadImpl::current()->RequestNewCompositorFrameSink(
      fallback, routing_id_, frame_swap_message_queue_,
      GetURLForGraphicsContext3D(), callback);
}

void RenderWidget::DidCommitAndDrawCompositorFrame() {
  // NOTE: Tests may break if this event is renamed or moved. See
  // tab_capture_performancetest.cc.
  TRACE_EVENT0("gpu", "RenderWidget::DidCommitAndDrawCompositorFrame");

  for (auto& observer : render_frames_)
    observer.DidCommitAndDrawCompositorFrame();

  // Notify subclasses that we initiated the paint operation.
  DidInitiatePaint();
}

void RenderWidget::DidCommitCompositorFrame() {
  for (auto& observer : render_frames_)
    observer.DidCommitCompositorFrame();
  for (auto& observer : render_frame_proxies_)
    observer.DidCommitCompositorFrame();
}

void RenderWidget::DidCompletePageScaleAnimation() {}

void RenderWidget::DidReceiveCompositorFrameAck() {
  TRACE_EVENT0("renderer", "RenderWidget::DidReceiveCompositorFrameAck");

  if (!next_paint_flags_ && !need_update_rect_for_auto_resize_) {
    return;
  }

  ViewHostMsg_UpdateRect_Params params;
  params.view_size = size_;
  params.flags = next_paint_flags_;

  Send(new ViewHostMsg_UpdateRect(routing_id_, params));
  next_paint_flags_ = 0;
  need_update_rect_for_auto_resize_ = false;
}

bool RenderWidget::IsClosing() const {
  return host_closing_;
}

void RenderWidget::RequestScheduleAnimation() {
  ScheduleAnimation();
}

void RenderWidget::UpdateVisualState() {
  GetWebWidget()->UpdateAllLifecyclePhases();
  GetWebWidget()->SetSuppressFrameRequestsWorkaroundFor704763Only(false);

  if (time_to_first_active_paint_recorded_)
    return;

  RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
  if (!render_thread_impl->NeedsToRecordFirstActivePaint())
    return;

  time_to_first_active_paint_recorded_ = true;
  base::TimeDelta sample = base::TimeTicks::Now() - was_shown_time_;
  UMA_HISTOGRAM_TIMES("PurgeAndSuspend.Experimental.TimeToFirstActivePaint",
                      sample);
}

void RenderWidget::WillBeginCompositorFrame() {
  TRACE_EVENT0("gpu", "RenderWidget::willBeginCompositorFrame");

  GetWebWidget()->SetSuppressFrameRequestsWorkaroundFor704763Only(true);

  // The UpdateTextInputState can result in further layout and possibly
  // enable GPU acceleration so they need to be called before any painting
  // is done.
  UpdateTextInputState();
  UpdateSelectionBounds();

  for (auto& observer : render_frame_proxies_)
    observer.WillBeginCompositorFrame();
}

std::unique_ptr<cc::SwapPromise> RenderWidget::RequestCopyOfOutputForLayoutTest(
    std::unique_ptr<cc::CopyOutputRequest> request) {
  return RenderThreadImpl::current()->RequestCopyOfOutputForLayoutTest(
      routing_id_, std::move(request));
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetInputHandlerDelegate

void RenderWidget::FocusChangeComplete() {
  if (!GetWebWidget()->IsWebFrameWidget())
    return;
  blink::WebLocalFrame* focused =
      static_cast<blink::WebFrameWidget*>(GetWebWidget())
          ->LocalRoot()
          ->View()
          ->FocusedFrame();
  if (focused && focused->AutofillClient())
    focused->AutofillClient()->DidCompleteFocusChangeInFrame();
}

bool RenderWidget::HasTouchEventHandlersAt(const gfx::Point& point) const {
  if (owner_delegate_)
    return owner_delegate_->DoesRenderWidgetHaveTouchEventHandlersAt(point);

  return true;
}

void RenderWidget::ObserveGestureEventAndResult(
    const blink::WebGestureEvent& gesture_event,
    const gfx::Vector2dF& unused_delta,
    bool event_processed) {
  if (!compositor_deps_->IsElasticOverscrollEnabled())
    return;

  cc::InputHandlerScrollResult scroll_result;
  scroll_result.did_scroll = event_processed;
  scroll_result.did_overscroll_root = !unused_delta.IsZero();
  scroll_result.unused_scroll_delta = unused_delta;

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  InputHandlerManager* input_handler_manager =
      render_thread ? render_thread->input_handler_manager() : NULL;
  if (input_handler_manager) {
    input_handler_manager->ObserveGestureEventAndResultOnMainThread(
        routing_id_, gesture_event, scroll_result);
  }
}

void RenderWidget::OnDidHandleKeyEvent() {
  ClearEditCommands();
}

void RenderWidget::SetEditCommandForNextKeyEvent(const std::string& name,
                                                 const std::string& value) {
  ClearEditCommands();
  edit_commands_.emplace_back(name, value);
}

void RenderWidget::ClearEditCommands() {
  edit_commands_.clear();
}

void RenderWidget::OnDidOverscroll(const ui::DidOverscrollParams& params) {
  Send(new InputHostMsg_DidOverscroll(routing_id_, params));
}

void RenderWidget::OnInputEventAck(
    std::unique_ptr<InputEventAck> input_event_ack) {
  SendOrCrash(
      new InputHostMsg_HandleInputEvent_ACK(routing_id_, *input_event_ack));
}

void RenderWidget::SetInputHandler(RenderWidgetInputHandler* input_handler) {
  // Nothing to do here. RenderWidget created the |input_handler| and will take
  // ownership of it. We just verify here that we don't already have an input
  // handler.
  DCHECK(!input_handler_);
}

void RenderWidget::ShowVirtualKeyboard() {
  UpdateTextInputStateInternal(true, false);
}

void RenderWidget::ClearTextInputState() {
  text_input_info_ = blink::WebTextInputInfo();
  text_input_type_ = ui::TextInputType::TEXT_INPUT_TYPE_NONE;
  text_input_mode_ = ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT;
  can_compose_inline_ = false;
  text_input_flags_ = 0;
}

void RenderWidget::UpdateTextInputState() {
  UpdateTextInputStateInternal(false, false);
}

void RenderWidget::UpdateTextInputStateInternal(bool show_virtual_keyboard,
                                                bool reply_to_request) {
  TRACE_EVENT0("renderer", "RenderWidget::UpdateTextInputState");

  if (ime_event_guard_) {
    DCHECK(!reply_to_request);
    // show_virtual_keyboard should still be effective even if it was set inside
    // the IME
    // event guard.
    if (show_virtual_keyboard)
      ime_event_guard_->set_show_virtual_keyboard(true);
    return;
  }

  ui::TextInputType new_type = GetTextInputType();
  if (IsDateTimeInput(new_type))
    return;  // Not considered as a text input field in WebKit/Chromium.

  blink::WebTextInputInfo new_info;
  if (auto* controller = GetInputMethodController())
    new_info = controller->TextInputInfo();
  const ui::TextInputMode new_mode =
      ConvertWebTextInputMode(new_info.input_mode);

  bool new_can_compose_inline = CanComposeInline();

  // Only sends text input params if they are changed or if the ime should be
  // shown.
  if (show_virtual_keyboard || reply_to_request ||
      text_input_type_ != new_type || text_input_mode_ != new_mode ||
      text_input_info_ != new_info ||
      can_compose_inline_ != new_can_compose_inline) {
    TextInputState params;
    params.type = new_type;
    params.mode = new_mode;
    params.flags = new_info.flags;
    params.value = new_info.value.Utf8();
    params.selection_start = new_info.selection_start;
    params.selection_end = new_info.selection_end;
    params.composition_start = new_info.composition_start;
    params.composition_end = new_info.composition_end;
    params.can_compose_inline = new_can_compose_inline;
    // TODO(changwan): change instances of show_ime_if_needed to
    // show_virtual_keyboard.
    params.show_ime_if_needed = show_virtual_keyboard;
    params.reply_to_request = reply_to_request;
    Send(new ViewHostMsg_TextInputStateChanged(routing_id(), params));

    text_input_info_ = new_info;
    text_input_type_ = new_type;
    text_input_mode_ = new_mode;
    can_compose_inline_ = new_can_compose_inline;
    text_input_flags_ = new_info.flags;
  }
}

bool RenderWidget::WillHandleGestureEvent(const blink::WebGestureEvent& event) {
  possible_drag_event_info_.event_source =
      ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH;
  possible_drag_event_info_.event_location =
      gfx::Point(event.global_x, event.global_y);

  return false;
}

bool RenderWidget::WillHandleMouseEvent(const blink::WebMouseEvent& event) {
  for (auto& observer : render_frames_)
    observer.RenderWidgetWillHandleMouseEvent();

  possible_drag_event_info_.event_source =
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE;
  possible_drag_event_info_.event_location =
      gfx::Point(event.PositionInScreen().x, event.PositionInScreen().y);

  if (owner_delegate_)
    return owner_delegate_->RenderWidgetWillHandleMouseEvent(event);

  return false;
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetScreenMetricsDelegate

void RenderWidget::Redraw() {
  set_next_paint_is_resize_ack();
  if (compositor_)
    compositor_->SetNeedsRedrawRect(gfx::Rect(size_));
}

void RenderWidget::ResizeWebWidget() {
  GetWebWidget()->Resize(GetSizeForWebWidget());
}

gfx::Size RenderWidget::GetSizeForWebWidget() const {
  if (IsUseZoomForDSFEnabled())
    return gfx::ScaleToCeiledSize(size_, GetOriginalDeviceScaleFactor());

  return size_;
}

void RenderWidget::Resize(const ResizeParams& params) {
  bool orientation_changed =
      screen_info_.orientation_angle != params.screen_info.orientation_angle ||
      screen_info_.orientation_type != params.screen_info.orientation_type;

  screen_info_ = params.screen_info;

  if (device_scale_factor_ != screen_info_.device_scale_factor) {
    device_scale_factor_ = screen_info_.device_scale_factor;
    OnDeviceScaleFactorChanged();
  }

  if (resizing_mode_selector_->NeverUsesSynchronousResize()) {
    // A resize ack shouldn't be requested if we have not ACK'd the previous
    // one.
    DCHECK(!params.needs_resize_ack || !next_paint_is_resize_ack());
  }

  // Ignore this during shutdown.
  if (!GetWebWidget())
    return;

  if (params.local_surface_id)
    local_surface_id_ = *params.local_surface_id;

  if (compositor_) {
    compositor_->SetViewportSize(params.physical_backing_size);
    compositor_->setBottomControlsHeight(params.bottom_controls_height);
    compositor_->SetRasterColorSpace(
        screen_info_.icc_profile.GetParametricColorSpace());
    // If surface synchronization is enable, then this will use the provided
    // |local_surface_id_| to submit the next generated CompositorFrame.
    // If the ID is not valid, then the compositor will defer commits until
    // it receives a valid surface ID. This is a no-op if surface
    // synchronization is disabled.
    compositor_->SetLocalSurfaceId(local_surface_id_);
  }

  visible_viewport_size_ = params.visible_viewport_size;

  // NOTE: We may have entered fullscreen mode without changing our size.
  bool fullscreen_change =
      is_fullscreen_granted_ != params.is_fullscreen_granted;
  is_fullscreen_granted_ = params.is_fullscreen_granted;
  display_mode_ = params.display_mode;

  size_ = params.new_size;
  physical_backing_size_ = params.physical_backing_size;

  ResizeWebWidget();

  WebSize visual_viewport_size;

  if (IsUseZoomForDSFEnabled()) {
    visual_viewport_size = gfx::ScaleToCeiledSize(
        params.visible_viewport_size,
        GetOriginalDeviceScaleFactor());
  } else {
    visual_viewport_size = visible_viewport_size_;
  }

  GetWebWidget()->ResizeVisualViewport(visual_viewport_size);

  // When resizing, we want to wait to paint before ACK'ing the resize.  This
  // ensures that we only resize as fast as we can paint.  We only need to
  // send an ACK if we are resized to a non-empty rect.
  if (params.new_size.IsEmpty() || params.physical_backing_size.IsEmpty()) {
    // In this case there is no paint/composite and therefore no
    // ViewHostMsg_UpdateRect to send the resize ack with. We'd need to send the
    // ack through a fake ViewHostMsg_UpdateRect or a different message.
    DCHECK(!params.needs_resize_ack);
  }

  // Send the Resize_ACK flag once we paint again if requested.
  if (params.needs_resize_ack)
    set_next_paint_is_resize_ack();

  if (fullscreen_change)
    DidToggleFullscreen();

  if (orientation_changed)
    OnOrientationChange();

  // If a resize ack is requested and it isn't set-up, then no more resizes will
  // come in and in general things will go wrong.
  DCHECK(!params.needs_resize_ack || next_paint_is_resize_ack());
}

void RenderWidget::SetScreenMetricsEmulationParameters(
    bool enabled,
    const blink::WebDeviceEmulationParams& params) {
  // This is only supported in RenderView.
  NOTREACHED();
}

void RenderWidget::SetScreenRects(const gfx::Rect& view_screen_rect,
                                  const gfx::Rect& window_screen_rect) {
  view_screen_rect_ = view_screen_rect;
  window_screen_rect_ = window_screen_rect;
}

///////////////////////////////////////////////////////////////////////////////
// WebWidgetClient

void RenderWidget::AutoResizeCompositor()  {
  physical_backing_size_ = gfx::ScaleToCeiledSize(size_, device_scale_factor_);
  if (compositor_)
    compositor_->SetViewportSize(physical_backing_size_);
}

blink::WebLayerTreeView* RenderWidget::InitializeLayerTreeView() {
  DCHECK(!host_closing_);

  compositor_ = RenderWidgetCompositor::Create(this, compositor_deps_);
  auto animation_host = cc::AnimationHost::CreateMainInstance();

  // Oopif status must be set before the LayerTreeHost is created.
  compositor_->SetIsForOopif(for_oopif_);
  auto layer_tree_host = RenderWidgetCompositor::CreateLayerTreeHost(
      compositor_.get(), compositor_.get(), animation_host.get(),
      compositor_deps_, device_scale_factor_, screen_info_);
  compositor_->Initialize(std::move(layer_tree_host),
                          std::move(animation_host));

  compositor_->SetViewportSize(physical_backing_size_);
  OnDeviceScaleFactorChanged();
  compositor_->SetRasterColorSpace(
      screen_info_.icc_profile.GetParametricColorSpace());
  compositor_->SetContentSourceId(current_content_source_id_);
  compositor_->SetLocalSurfaceId(local_surface_id_);
  // For background pages and certain tests, we don't want to trigger
  // CompositorFrameSink creation.
  bool should_generate_frame_sink =
      !compositor_never_visible_ && RenderThreadImpl::current();
  if (!should_generate_frame_sink)
    compositor_->SetNeverVisible();

  StartCompositor();
  DCHECK_NE(MSG_ROUTING_NONE, routing_id_);
  compositor_->SetFrameSinkId(
      cc::FrameSinkId(RenderThread::Get()->GetClientId(), routing_id_));

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  // render_thread may be NULL in tests.
  InputHandlerManager* input_handler_manager =
      render_thread ? render_thread->input_handler_manager() : NULL;
  if (input_handler_manager) {
    input_event_queue_ = new MainThreadEventQueue(
        this, render_thread->GetRendererScheduler()->CompositorTaskRunner(),
        render_thread->GetRendererScheduler(), should_generate_frame_sink);
    input_handler_manager->AddInputHandler(
        routing_id_, compositor()->GetInputHandler(), input_event_queue_,
        weak_ptr_factory_.GetWeakPtr(),
        compositor_deps_->IsScrollAnimatorEnabled());
    has_added_input_handler_ = true;
  }

  return compositor_.get();
}

void RenderWidget::WillCloseLayerTreeView() {
  if (host_closing_)
    return;

  // Prevent new compositors or output surfaces from being created.
  host_closing_ = true;

  // Always send this notification to prevent new layer tree views from
  // being created, even if one hasn't been created yet.
  if (blink::WebWidget* widget = GetWebWidget())
    widget->WillCloseLayerTreeView();
}

void RenderWidget::DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) {
  if (layout_type == blink::WebMeaningfulLayout::kVisuallyNonEmpty) {
    QueueMessage(new ViewHostMsg_DidFirstVisuallyNonEmptyPaint(routing_id_),
                 MESSAGE_DELIVERY_POLICY_WITH_VISUAL_STATE);
  }

  for (auto& observer : render_frames_)
    observer.DidMeaningfulLayout(layout_type);
}

// static
std::unique_ptr<cc::SwapPromise> RenderWidget::QueueMessageImpl(
    IPC::Message* msg,
    MessageDeliveryPolicy policy,
    FrameSwapMessageQueue* frame_swap_message_queue,
    scoped_refptr<IPC::SyncMessageFilter> sync_message_filter,
    int source_frame_number) {
  bool first_message_for_frame = false;
  frame_swap_message_queue->QueueMessageForFrame(policy, source_frame_number,
                                                 base::WrapUnique(msg),
                                                 &first_message_for_frame);
  if (first_message_for_frame) {
    std::unique_ptr<cc::SwapPromise> promise(new QueueMessageSwapPromise(
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

  std::unique_ptr<cc::SwapPromise> swap_promise =
      QueueMessageImpl(msg, policy, frame_swap_message_queue_.get(),
                       RenderThreadImpl::current()->sync_message_filter(),
                       compositor_->GetSourceFrameNumber());

  if (swap_promise) {
    compositor_->QueueSwapPromise(std::move(swap_promise));
    // Request a main frame. This might either A) request a commit ahead of time
    // or B) request a commit which is not needed because there are not pending
    // updates. If B) then the frame will be aborted early and the swap promises
    // will be broken (see EarlyOut_NoUpdates).
    compositor_->SetNeedsBeginFrame();
  }
}

void RenderWidget::DidChangeCursor(const WebCursorInfo& cursor_info) {
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
void RenderWidget::Show(WebNavigationPolicy policy) {
  DCHECK(!did_show_) << "received extraneous Show call";
  DCHECK(routing_id_ != MSG_ROUTING_NONE);
  DCHECK(!show_callback_.is_null());

  if (did_show_)
    return;

  did_show_ = true;

  // The opener is responsible for actually showing this widget.
  show_callback_.Run(this, policy, initial_rect_);
  show_callback_.Reset();

  // NOTE: initial_rect_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if as_popup is false, or the browser
  // process will impose a default position otherwise.
  SetPendingWindowRect(initial_rect_);
}

void RenderWidget::DoDeferredClose() {
  WillCloseLayerTreeView();
  Send(new ViewHostMsg_Close(routing_id_));
}

void RenderWidget::NotifyOnClose() {
  for (auto& observer : render_frames_)
    observer.WidgetWillClose();
}

void RenderWidget::CloseWidgetSoon() {
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
    std::unique_ptr<SyntheticGestureParams> gesture_params,
    const SyntheticGestureCompletionCallback& callback) {
  DCHECK(!callback.is_null());

  pending_synthetic_gesture_callbacks_.push(callback);

  SyntheticGesturePacket gesture_packet;
  gesture_packet.set_gesture_params(std::move(gesture_params));

  Send(new InputHostMsg_QueueSyntheticGesture(routing_id_, gesture_packet));
}

void RenderWidget::Close() {
  screen_metrics_emulator_.reset();
  WillCloseLayerTreeView();
  compositor_.reset();
  if (webwidget_internal_) {
    webwidget_internal_->Close();
    webwidget_internal_ = nullptr;
  }
}

void RenderWidget::ScreenRectToEmulatedIfNeeded(WebRect* window_rect) const {
  DCHECK(window_rect);
  float scale = popup_origin_scale_for_emulation_;
  if (!scale)
    return;
  window_rect->x =
      popup_view_origin_for_emulation_.x() +
      (window_rect->x - popup_screen_origin_for_emulation_.x()) / scale;
  window_rect->y =
      popup_view_origin_for_emulation_.y() +
      (window_rect->y - popup_screen_origin_for_emulation_.y()) / scale;
}

void RenderWidget::EmulatedToScreenRectIfNeeded(WebRect* window_rect) const {
  DCHECK(window_rect);
  float scale = popup_origin_scale_for_emulation_;
  if (!scale)
    return;
  window_rect->x =
      popup_screen_origin_for_emulation_.x() +
      (window_rect->x - popup_view_origin_for_emulation_.x()) * scale;
  window_rect->y =
      popup_screen_origin_for_emulation_.y() +
      (window_rect->y - popup_view_origin_for_emulation_.y()) * scale;
}

WebRect RenderWidget::WindowRect() {
  WebRect rect;
  if (pending_window_rect_count_) {
    // NOTE(mbelshe): If there is a pending_window_rect_, then getting
    // the RootWindowRect is probably going to return wrong results since the
    // browser may not have processed the Move yet.  There isn't really anything
    // good to do in this case, and it shouldn't happen - since this size is
    // only really needed for windowToScreen, which is only used for Popups.
    rect = pending_window_rect_;
  } else {
    rect = window_screen_rect_;
  }

  ScreenRectToEmulatedIfNeeded(&rect);
  return rect;
}

WebRect RenderWidget::ViewRect() {
  WebRect rect = view_screen_rect_;
  ScreenRectToEmulatedIfNeeded(&rect);
  return rect;
}

void RenderWidget::SetToolTipText(const blink::WebString& text,
                                  WebTextDirection hint) {
  Send(new ViewHostMsg_SetTooltipText(routing_id_, text.Utf16(), hint));
}

void RenderWidget::SetWindowRect(const WebRect& rect_in_screen) {
  WebRect window_rect = rect_in_screen;
  EmulatedToScreenRectIfNeeded(&window_rect);

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

  // Popups don't get size updates back from the browser so just store the set
  // values.
  if (popup_type_ != blink::kWebPopupTypeNone) {
    window_screen_rect_ = rect;
    view_screen_rect_ = rect;
  }
}

void RenderWidget::OnImeSetComposition(
    const base::string16& text,
    const std::vector<WebCompositionUnderline>& underlines,
    const gfx::Range& replacement_range,
    int selection_start, int selection_end) {
  if (!ShouldHandleImeEvents())
    return;

#if BUILDFLAG(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    focused_pepper_plugin_->render_frame()->OnImeSetComposition(
        text, underlines, selection_start, selection_end);
    return;
  }
#endif
  ImeEventGuard guard(this);
  blink::WebInputMethodController* controller = GetInputMethodController();
  if (!controller ||
      !controller->SetComposition(
          WebString::FromUTF16(text),
          WebVector<WebCompositionUnderline>(underlines),
          replacement_range.IsValid()
              ? WebRange(replacement_range.start(), replacement_range.length())
              : WebRange(),
          selection_start, selection_end)) {
    // If we failed to set the composition text, then we need to let the browser
    // process to cancel the input method's ongoing composition session, to make
    // sure we are in a consistent state.
    Send(new InputHostMsg_ImeCancelComposition(routing_id()));
  }
  UpdateCompositionInfo(false /* not an immediate request */);
}

void RenderWidget::OnImeCommitText(
    const base::string16& text,
    const std::vector<WebCompositionUnderline>& underlines,
    const gfx::Range& replacement_range,
    int relative_cursor_pos) {
  if (!ShouldHandleImeEvents())
    return;

#if BUILDFLAG(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    focused_pepper_plugin_->render_frame()->OnImeCommitText(
        text, replacement_range, relative_cursor_pos);
    return;
  }
#endif
  ImeEventGuard guard(this);
  input_handler_->set_handling_input_event(true);
  if (auto* controller = GetInputMethodController()) {
    controller->CommitText(
        WebString::FromUTF16(text),
        WebVector<WebCompositionUnderline>(underlines),
        replacement_range.IsValid()
            ? WebRange(replacement_range.start(), replacement_range.length())
            : WebRange(),
        relative_cursor_pos);
  }
  input_handler_->set_handling_input_event(false);
  UpdateCompositionInfo(false /* not an immediate request */);
}

void RenderWidget::OnImeFinishComposingText(bool keep_selection) {
  if (!ShouldHandleImeEvents())
    return;

#if BUILDFLAG(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    focused_pepper_plugin_->render_frame()->OnImeFinishComposingText(
        keep_selection);
    return;
  }
#endif

  if (!GetWebWidget())
    return;
  ImeEventGuard guard(this);
  input_handler_->set_handling_input_event(true);
  if (auto* controller = GetInputMethodController()) {
    controller->FinishComposingText(
        keep_selection ? WebInputMethodController::kKeepSelection
                       : WebInputMethodController::kDoNotKeepSelection);
  }
  input_handler_->set_handling_input_event(false);
  UpdateCompositionInfo(false /* not an immediate request */);
}

void RenderWidget::OnDeviceScaleFactorChanged() {
  if (!compositor_)
    return;
  if (IsUseZoomForDSFEnabled())
    compositor_->SetPaintedDeviceScaleFactor(GetOriginalDeviceScaleFactor());
  else
    compositor_->SetDeviceScaleFactor(device_scale_factor_);
}

void RenderWidget::OnRepaint(gfx::Size size_to_paint) {
  // During shutdown we can just ignore this message.
  if (!GetWebWidget())
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
  if (!GetWebWidget())
    return;
  GetWebWidget()->SetTextDirection(direction);
}

void RenderWidget::OnUpdateScreenRects(const gfx::Rect& view_screen_rect,
                                       const gfx::Rect& window_screen_rect) {
  if (screen_metrics_emulator_) {
    screen_metrics_emulator_->OnUpdateScreenRects(view_screen_rect,
                                                  window_screen_rect);
  } else {
    SetScreenRects(view_screen_rect, window_screen_rect);
  }
  Send(new ViewHostMsg_UpdateScreenRects_ACK(routing_id()));
}

void RenderWidget::OnUpdateWindowScreenRect(
    const gfx::Rect& window_screen_rect) {
  if (screen_metrics_emulator_)
    screen_metrics_emulator_->OnUpdateWindowScreenRect(window_screen_rect);
  else
    window_screen_rect_ = window_screen_rect;
}

void RenderWidget::OnSetViewportIntersection(
    const gfx::Rect& viewport_intersection) {
  if (GetWebWidget() && GetWebWidget()->IsWebFrameWidget()) {
    DCHECK(popup_type_ == WebPopupType::kWebPopupTypeNone);
    static_cast<WebFrameWidget*>(GetWebWidget())
        ->SetRemoteViewportIntersection(viewport_intersection);
  }
}

void RenderWidget::OnDragTargetDragEnter(
    const std::vector<DropData::Metadata>& drop_meta_data,
    const gfx::Point& client_point,
    const gfx::Point& screen_point,
    WebDragOperationsMask ops,
    int key_modifiers) {
  if (!GetWebWidget())
    return;

  DCHECK(GetWebWidget()->IsWebFrameWidget());
  WebDragOperation operation =
      static_cast<WebFrameWidget*>(GetWebWidget())
          ->DragTargetDragEnter(DropMetaDataToWebDragData(drop_meta_data),
                                client_point, screen_point, ops, key_modifiers);

  Send(new DragHostMsg_UpdateDragCursor(routing_id(), operation));
}

void RenderWidget::OnDragTargetDragOver(const gfx::Point& client_point,
                                        const gfx::Point& screen_point,
                                        WebDragOperationsMask ops,
                                        int key_modifiers) {
  if (!GetWebWidget())
    return;

  DCHECK(GetWebWidget()->IsWebFrameWidget());
  WebDragOperation operation =
      static_cast<WebFrameWidget*>(GetWebWidget())
          ->DragTargetDragOver(ConvertWindowPointToViewport(client_point),
                               screen_point, ops, key_modifiers);

  Send(new DragHostMsg_UpdateDragCursor(routing_id(), operation));
}

void RenderWidget::OnDragTargetDragLeave(const gfx::Point& client_point,
                                         const gfx::Point& screen_point) {
  if (!GetWebWidget())
    return;
  DCHECK(GetWebWidget()->IsWebFrameWidget());
  static_cast<WebFrameWidget*>(GetWebWidget())
      ->DragTargetDragLeave(ConvertWindowPointToViewport(client_point),
                            screen_point);
}

void RenderWidget::OnDragTargetDrop(const DropData& drop_data,
                                    const gfx::Point& client_point,
                                    const gfx::Point& screen_point,
                                    int key_modifiers) {
  if (!GetWebWidget())
    return;

  DCHECK(GetWebWidget()->IsWebFrameWidget());
  static_cast<WebFrameWidget*>(GetWebWidget())
      ->DragTargetDrop(DropDataToWebDragData(drop_data),
                       ConvertWindowPointToViewport(client_point), screen_point,
                       key_modifiers);
}

void RenderWidget::OnDragSourceEnded(const gfx::Point& client_point,
                                     const gfx::Point& screen_point,
                                     WebDragOperation op) {
  if (!GetWebWidget())
    return;

  static_cast<WebFrameWidget*>(GetWebWidget())
      ->DragSourceEndedAt(ConvertWindowPointToViewport(client_point),
                          screen_point, op);
}

void RenderWidget::OnDragSourceSystemDragEnded() {
  if (!GetWebWidget())
    return;

  static_cast<WebFrameWidget*>(GetWebWidget())->DragSourceSystemDragEnded();
}

void RenderWidget::ShowVirtualKeyboardOnElementFocus() {
#if defined(OS_CHROMEOS)
  // On ChromeOS, virtual keyboard is triggered only when users leave the
  // mouse button or the finger and a text input element is focused at that
  // time. Focus event itself shouldn't trigger virtual keyboard.
  UpdateTextInputState();
#else
  ShowVirtualKeyboard();
#endif

// TODO(rouslan): Fix ChromeOS and Windows 8 behavior of autofill popup with
// virtual keyboard.
#if !defined(OS_ANDROID)
  FocusChangeComplete();
#endif
}

ui::TextInputType RenderWidget::GetTextInputType() {
#if BUILDFLAG(ENABLE_PLUGINS)
  if (focused_pepper_plugin_)
    return focused_pepper_plugin_->text_input_type();
#endif
  if (auto* controller = GetInputMethodController())
    return ConvertWebTextInputType(controller->TextInputType());
  return ui::TEXT_INPUT_TYPE_NONE;
}

void RenderWidget::UpdateCompositionInfo(bool immediate_request) {
  if (!monitor_composition_info_ && !immediate_request)
    return;  // Do not calculate composition info if not requested.

  TRACE_EVENT0("renderer", "RenderWidget::UpdateCompositionInfo");
  gfx::Range range;
  std::vector<gfx::Rect> character_bounds;

  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE) {
    // Composition information is only available on editable node.
    range = gfx::Range::InvalidRange();
  } else {
    GetCompositionRange(&range);
    GetCompositionCharacterBounds(&character_bounds);
  }

  if (!immediate_request &&
      !ShouldUpdateCompositionInfo(range, character_bounds)) {
    return;
  }
  composition_character_bounds_ = character_bounds;
  composition_range_ = range;
  Send(new InputHostMsg_ImeCompositionRangeChanged(
      routing_id(), composition_range_, composition_character_bounds_));
}

void RenderWidget::ConvertViewportToWindow(blink::WebRect* rect) {
  if (IsUseZoomForDSFEnabled()) {
    float reverse = 1 / GetOriginalDeviceScaleFactor();
    // TODO(oshima): We may need to allow pixel precision here as the the
    // anchor element can be placed at half pixel.
    gfx::Rect window_rect =
        gfx::ScaleToEnclosedRect(gfx::Rect(*rect), reverse);
    rect->x = window_rect.x();
    rect->y = window_rect.y();
    rect->width = window_rect.width();
    rect->height = window_rect.height();
  }
}

void RenderWidget::ConvertWindowToViewport(blink::WebFloatRect* rect) {
  if (IsUseZoomForDSFEnabled()) {
    rect->x *= GetOriginalDeviceScaleFactor();
    rect->y *= GetOriginalDeviceScaleFactor();
    rect->width *= GetOriginalDeviceScaleFactor();
    rect->height *= GetOriginalDeviceScaleFactor();
  }
}

#if defined(OS_ANDROID)
void RenderWidget::OnRequestTextInputStateUpdate() {
  DCHECK(!ime_event_guard_);
  UpdateSelectionBounds();
  UpdateTextInputStateInternal(false, true /* reply_to_request */);
}
#endif

void RenderWidget::OnRequestCompositionUpdates(bool immediate_request,
                                               bool monitor_updates) {
  monitor_composition_info_ = monitor_updates;
  if (!immediate_request)
    return;
  UpdateCompositionInfo(true /* immediate request */);
}

void RenderWidget::OnSetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor_ == device_scale_factor)
    return;

  device_scale_factor_ = device_scale_factor;
  OnDeviceScaleFactorChanged();
  physical_backing_size_ = gfx::ScaleToCeiledSize(size_, device_scale_factor_);
}

void RenderWidget::OnOrientationChange() {
  WebWidget* web_widget = GetWebWidget();
  if (web_widget && web_widget->IsWebFrameWidget()) {
    WebFrameWidget* web_frame_widget = static_cast<WebFrameWidget*>(web_widget);
    web_frame_widget->LocalRoot()->SendOrientationChangeEvent();
  }
}

void RenderWidget::SetHidden(bool hidden) {
  if (is_hidden_ == hidden)
    return;

  // The status has changed.  Tell the RenderThread about it and ensure
  // throttled acks are released in case frame production ceases.
  is_hidden_ = hidden;

  if (is_hidden_) {
    RenderThreadImpl::current()->WidgetHidden();
    time_to_first_active_paint_recorded_ = false;
  } else
    RenderThreadImpl::current()->WidgetRestored();

  if (render_widget_scheduling_state_)
    render_widget_scheduling_state_->SetHidden(hidden);
}

void RenderWidget::DidToggleFullscreen() {
  if (!GetWebWidget())
    return;

  if (is_fullscreen_granted_) {
    GetWebWidget()->DidEnterFullscreen();
  } else {
    GetWebWidget()->DidExitFullscreen();
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

void RenderWidget::OnImeEventGuardStart(ImeEventGuard* guard) {
  if (!ime_event_guard_)
    ime_event_guard_ = guard;
}

void RenderWidget::OnImeEventGuardFinish(ImeEventGuard* guard) {
  if (ime_event_guard_ != guard) {
    DCHECK(!ime_event_guard_->reply_to_request());
    return;
  }
  ime_event_guard_ = nullptr;

  // While handling an ime event, text input state and selection bounds updates
  // are ignored. These must explicitly be updated once finished handling the
  // ime event.
  UpdateSelectionBounds();
#if defined(OS_ANDROID)
  if (guard->show_virtual_keyboard())
    ShowVirtualKeyboard();
  else
    UpdateTextInputState();
#endif
}

void RenderWidget::GetSelectionBounds(gfx::Rect* focus, gfx::Rect* anchor) {
#if BUILDFLAG(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    // TODO(kinaba) http://crbug.com/101101
    // Current Pepper IME API does not handle selection bounds. So we simply
    // use the caret position as an empty range for now. It will be updated
    // after Pepper API equips features related to surrounding text retrieval.
    blink::WebRect caret(focused_pepper_plugin_->GetCaretBounds());
    ConvertViewportToWindow(&caret);
    *focus = caret;
    *anchor = caret;
    return;
  }
#endif
  WebRect focus_webrect;
  WebRect anchor_webrect;
  GetWebWidget()->SelectionBounds(focus_webrect, anchor_webrect);
  ConvertViewportToWindow(&focus_webrect);
  ConvertViewportToWindow(&anchor_webrect);
  *focus = focus_webrect;
  *anchor = anchor_webrect;
}

void RenderWidget::UpdateSelectionBounds() {
  TRACE_EVENT0("renderer", "RenderWidget::UpdateSelectionBounds");
  if (!GetWebWidget())
    return;
  if (ime_event_guard_)
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
      !blink::WebRuntimeFeatures::IsCompositedSelectionUpdateEnabled();
#endif
  if (send_ipc) {
    ViewHostMsg_SelectionBounds_Params params;
    GetSelectionBounds(&params.anchor_rect, &params.focus_rect);
    if (selection_anchor_rect_ != params.anchor_rect ||
        selection_focus_rect_ != params.focus_rect) {
      selection_anchor_rect_ = params.anchor_rect;
      selection_focus_rect_ = params.focus_rect;
      GetWebWidget()->SelectionTextDirection(params.focus_dir,
                                             params.anchor_dir);
      params.is_anchor_first = GetWebWidget()->IsSelectionAnchorFirst();
      Send(new ViewHostMsg_SelectionBoundsChanged(routing_id_, params));
    }
  }

  UpdateCompositionInfo(false /* not an immediate request */);
}

void RenderWidget::DidAutoResize(const gfx::Size& new_size) {
  WebRect new_size_in_window(0, 0, new_size.width(), new_size.height());
  ConvertViewportToWindow(&new_size_in_window);
  if (size_.width() != new_size_in_window.width ||
      size_.height() != new_size_in_window.height) {
    size_ = gfx::Size(new_size_in_window.width, new_size_in_window.height);

    if (resizing_mode_selector_->is_synchronous_mode()) {
      gfx::Rect new_pos(WindowRect().x, WindowRect().y, size_.width(),
                        size_.height());
      view_screen_rect_ = new_pos;
      window_screen_rect_ = new_pos;
    }

    AutoResizeCompositor();

    if (!resizing_mode_selector_->is_synchronous_mode())
      need_update_rect_for_auto_resize_ = true;
  }
}

void RenderWidget::GetCompositionCharacterBounds(
    std::vector<gfx::Rect>* bounds) {
  DCHECK(bounds);
  bounds->clear();

#if BUILDFLAG(ENABLE_PLUGINS)
  if (focused_pepper_plugin_)
    return;
#endif

  if (!GetWebWidget())
    return;
  blink::WebVector<blink::WebRect> bounds_from_blink;
  if (!GetWebWidget()->GetCompositionCharacterBounds(bounds_from_blink))
    return;

  for (size_t i = 0; i < bounds_from_blink.size(); ++i) {
    ConvertViewportToWindow(&bounds_from_blink[i]);
    bounds->push_back(bounds_from_blink[i]);
  }
}

void RenderWidget::GetCompositionRange(gfx::Range* range) {
#if BUILDFLAG(ENABLE_PLUGINS)
  if (focused_pepper_plugin_)
    return;
#endif
  WebRange web_range = GetWebWidget()->CompositionRange();
  if (web_range.IsNull()) {
    *range = gfx::Range::InvalidRange();
    return;
  }
  range->set_start(web_range.StartOffset());
  range->set_end(web_range.EndOffset());
}

bool RenderWidget::ShouldUpdateCompositionInfo(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& bounds) {
  if (!range.IsValid())
    return false;
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

bool RenderWidget::CanComposeInline() {
#if BUILDFLAG(ENABLE_PLUGINS)
  if (focused_pepper_plugin_)
    return focused_pepper_plugin_->IsPluginAcceptingCompositionEvents();
#endif
  return true;
}

blink::WebScreenInfo RenderWidget::GetScreenInfo() {
  blink::WebScreenInfo web_screen_info;
  web_screen_info.device_scale_factor = screen_info_.device_scale_factor;
  web_screen_info.icc_profile = screen_info_.icc_profile;
  web_screen_info.depth = screen_info_.depth;
  web_screen_info.depth_per_component = screen_info_.depth_per_component;
  web_screen_info.is_monochrome = screen_info_.is_monochrome;
  web_screen_info.rect = blink::WebRect(screen_info_.rect);
  web_screen_info.available_rect = blink::WebRect(screen_info_.available_rect);
  switch (screen_info_.orientation_type) {
    case SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY:
      web_screen_info.orientation_type =
          blink::kWebScreenOrientationPortraitPrimary;
      break;
    case SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY:
      web_screen_info.orientation_type =
          blink::kWebScreenOrientationPortraitSecondary;
      break;
    case SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY:
      web_screen_info.orientation_type =
          blink::kWebScreenOrientationLandscapePrimary;
      break;
    case SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY:
      web_screen_info.orientation_type =
          blink::kWebScreenOrientationLandscapeSecondary;
      break;
    default:
      web_screen_info.orientation_type = blink::kWebScreenOrientationUndefined;
      break;
  }
  web_screen_info.orientation_angle = screen_info_.orientation_angle;
  return web_screen_info;
}

#if defined(OS_ANDROID)
void RenderWidget::ShowUnhandledTapUIIfNeeded(const WebPoint& tapped_position,
                                              const WebNode& tapped_node,
                                              bool page_changed) {
  DCHECK(input_handler_->handling_input_event());
  bool should_trigger = !page_changed && tapped_node.IsTextNode() &&
                        !tapped_node.IsContentEditable() &&
                        !tapped_node.IsInsideFocusableElementOrARIAWidget();
  if (should_trigger) {
    Send(new ViewHostMsg_ShowUnhandledTapUIIfNeeded(routing_id_,
        tapped_position.x, tapped_position.y));
  }
}
#endif

void RenderWidget::DidHandleGestureEvent(const WebGestureEvent& event,
                                         bool event_cancelled) {
#if defined(OS_ANDROID) || defined(USE_AURA)
  if (event_cancelled)
    return;
  if (event.GetType() == WebInputEvent::kGestureTap) {
    ShowVirtualKeyboard();
  } else if (event.GetType() == WebInputEvent::kGestureLongPress) {
    DCHECK(GetWebWidget());
    blink::WebInputMethodController* controller = GetInputMethodController();
    if (!controller || controller->TextInputInfo().value.IsEmpty())
      UpdateTextInputState();
    else
      ShowVirtualKeyboard();
  }
// TODO(ananta): Piggyback off existing IPCs to communicate this information,
// crbug/420130.
#if defined(OS_WIN)
  if (event.GetType() != blink::WebGestureEvent::kGestureTap)
    return;

  // TODO(estade): hit test the event against focused node to make sure
  // the tap actually hit the focused node.
  blink::WebInputMethodController* controller = GetInputMethodController();
  blink::WebTextInputType text_input_type =
      controller ? controller->TextInputType() : blink::kWebTextInputTypeNone;

  Send(new ViewHostMsg_FocusedNodeTouched(
      routing_id_, text_input_type != blink::kWebTextInputTypeNone));
#endif
#endif
}

void RenderWidget::DidOverscroll(
    const blink::WebFloatSize& overscrollDelta,
    const blink::WebFloatSize& accumulatedOverscroll,
    const blink::WebFloatPoint& position,
    const blink::WebFloatSize& velocity) {
#if defined(OS_MACOSX)
  // On OSX the user can disable the elastic overscroll effect. If that's the
  // case, don't forward the overscroll notification.
  DCHECK(compositor_deps());
  if (!compositor_deps()->IsElasticOverscrollEnabled())
    return;
#endif
  input_handler_->DidOverscrollFromBlink(overscrollDelta, accumulatedOverscroll,
                                         position, velocity);
}

void RenderWidget::StartCompositor() {
  if (!is_hidden())
    compositor_->SetVisible(true);
}

RenderWidgetCompositor* RenderWidget::compositor() const {
  return compositor_.get();
}

void RenderWidget::SetHandlingInputEventForTesting(bool handling_input_event) {
  input_handler_->set_handling_input_event(handling_input_event);
}

void RenderWidget::HasTouchEventHandlers(bool has_handlers) {
  if (render_widget_scheduling_state_)
    render_widget_scheduling_state_->SetHasTouchHandler(has_handlers);
  Send(new ViewHostMsg_HasTouchEventHandlers(routing_id_, has_handlers));
}

void RenderWidget::SetTouchAction(cc::TouchAction touch_action) {
  // Ignore setTouchAction calls that result from synthetic touch events (eg.
  // when blink is emulating touch with mouse).
  if (input_handler_->handling_event_type() != WebInputEvent::kTouchStart)
    return;

  Send(new InputHostMsg_SetTouchAction(routing_id_, touch_action));
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

void RenderWidget::OnWaitNextFrameForTests(int routing_id) {
  QueueMessage(new ViewHostMsg_WaitForNextFrameForTests_ACK(routing_id),
               MESSAGE_DELIVERY_POLICY_WITH_VISUAL_STATE);
}

float RenderWidget::GetOriginalDeviceScaleFactor() const {
  return
      screen_metrics_emulator_ ?
      screen_metrics_emulator_->original_screen_info().device_scale_factor :
      device_scale_factor_;
}

gfx::Point RenderWidget::ConvertWindowPointToViewport(
    const gfx::Point& point) {
  blink::WebFloatRect point_in_viewport(point.x(), point.y(), 0, 0);
  ConvertWindowToViewport(&point_in_viewport);
  return gfx::Point(point_in_viewport.x, point_in_viewport.y);
}

bool RenderWidget::RequestPointerLock() {
  return mouse_lock_dispatcher_->LockMouse(webwidget_mouse_lock_target_.get());
}

void RenderWidget::RequestPointerUnlock() {
  mouse_lock_dispatcher_->UnlockMouse(webwidget_mouse_lock_target_.get());
}

bool RenderWidget::IsPointerLocked() {
  return mouse_lock_dispatcher_->IsMouseLockedTo(
      webwidget_mouse_lock_target_.get());
}

void RenderWidget::StartDragging(blink::WebReferrerPolicy policy,
                                 const WebDragData& data,
                                 WebDragOperationsMask mask,
                                 const WebImage& image,
                                 const WebPoint& webImageOffset) {
  blink::WebRect offset_in_window(webImageOffset.x, webImageOffset.y, 0, 0);
  ConvertViewportToWindow(&offset_in_window);
  DropData drop_data(DropDataBuilder::Build(data));
  drop_data.referrer_policy = policy;
  gfx::Vector2d imageOffset(offset_in_window.x, offset_in_window.y);
  Send(new DragHostMsg_StartDragging(routing_id(), drop_data, mask,
                                     image.GetSkBitmap(), imageOffset,
                                     possible_drag_event_info_));
}

uint32_t RenderWidget::GetContentSourceId() {
  return current_content_source_id_;
}

void RenderWidget::IncrementContentSourceId() {
  if (compositor_)
    compositor_->SetContentSourceId(++current_content_source_id_);
}

blink::WebWidget* RenderWidget::GetWebWidget() const {
  return webwidget_internal_;
}

blink::WebInputMethodController* RenderWidget::GetInputMethodController()
    const {
  if (!GetWebWidget()->IsWebFrameWidget()) {
    // TODO(ekaramad): We should not get here in response to IME IPC or updates
    // when the RenderWidget is swapped out. We should top sending IPCs from the
    // browser side (https://crbug.com/669219).
    // If there is no WebFrameWidget, then there will be no
    // InputMethodControllers for a WebLocalFrame.
    return nullptr;
  }
  return static_cast<blink::WebFrameWidget*>(GetWebWidget())
      ->GetActiveWebInputMethodController();
}

}  // namespace content
