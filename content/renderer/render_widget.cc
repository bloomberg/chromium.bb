// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include <cmath>
#include <limits>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/base/switches.h"
#include "cc/input/touch_action.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/ukm_manager.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/switches.h"
#include "content/common/content_switches_internal.h"
#include "content/common/drag_event_source_info.h"
#include "content/common/drag_messages.h"
#include "content/common/render_frame_metadata.mojom.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/tab_switching_time_callback.h"
#include "content/common/text_input_state.h"
#include "content/common/widget_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/compositor/layer_tree_view.h"
#include "content/renderer/cursor_utils.h"
#include "content/renderer/devtools/render_widget_screen_metrics_emulator.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/external_popup_menu.h"
#include "content/renderer/frame_swap_message_queue.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/main_thread_event_queue.h"
#include "content/renderer/input/widget_input_handler_manager.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/queue_message_swap_promise.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_metadata_observer_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_sync_message.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/base/media_switches.h"
#include "ppapi/buildflags/buildflags.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_render_widget_scheduling_state.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_cursor_info.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_device_emulation_params.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_popup_menu_info.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_features.h"
#include "ui/native_theme/overlay_scrollbar_constants_aura.h"
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

using blink::WebImeTextSpan;
using blink::WebCursorInfo;
using blink::WebDeviceEmulationParams;
using blink::WebDragOperation;
using blink::WebDragOperationsMask;
using blink::WebDragData;
using blink::WebFrameWidget;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebInputEventResult;
using blink::WebInputMethodController;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebNavigationPolicy;
using blink::WebNode;
using blink::WebPagePopup;
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

RenderWidget::CreateRenderWidgetFunction g_create_render_widget_for_frame =
    nullptr;

using RoutingIDWidgetMap = std::map<int32_t, RenderWidget*>;
base::LazyInstance<RoutingIDWidgetMap>::Leaky g_routing_id_widget_map =
    LAZY_INSTANCE_INITIALIZER;

const base::Feature kUnpremultiplyAndDitherLowBitDepthTiles = {
    "UnpremultiplyAndDitherLowBitDepthTiles", base::FEATURE_ENABLED_BY_DEFAULT};

typedef std::map<std::string, ui::TextInputMode> TextInputModeMap;

static const int kInvalidNextPreviousFlagsValue = -1;
static const char* kOOPIF = "OOPIF";
static const char* kRenderer = "Renderer";

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

class ScopedUkmRafAlignedInputTimer {
 public:
  explicit ScopedUkmRafAlignedInputTimer(blink::WebWidget* webwidget)
      : webwidget_(webwidget) {
    webwidget_->BeginRafAlignedInput();
  }

  ~ScopedUkmRafAlignedInputTimer() { webwidget_->EndRafAlignedInput(); }

 private:
  blink::WebWidget* webwidget_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUkmRafAlignedInputTimer);
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
      if (base::EqualsASCII(meta_data_item.mime_type, ui::kMimeTypeURIList)) {
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
    item.string_type = WebString::FromUTF8(ui::kMimeTypeText);
    item.string_data = WebString::FromUTF16(drop_data.text.string());
    item_list.push_back(item);
  }

  if (!drop_data.url.is_empty()) {
    WebDragData::Item item;
    item.storage_type = WebDragData::Item::kStorageTypeString;
    item.string_type = WebString::FromUTF8(ui::kMimeTypeURIList);
    item.string_data = WebString::FromUTF8(drop_data.url.spec());
    item.title = WebString::FromUTF16(drop_data.url_title);
    item_list.push_back(item);
  }

  if (!drop_data.html.is_null()) {
    WebDragData::Item item;
    item.storage_type = WebDragData::Item::kStorageTypeString;
    item.string_type = WebString::FromUTF8(ui::kMimeTypeHTML);
    item.string_data = WebString::FromUTF16(drop_data.html.string());
    item.base_url = drop_data.html_base_url;
    item_list.push_back(item);
  }

  for (auto it = drop_data.filenames.begin(); it != drop_data.filenames.end();
       ++it) {
    WebDragData::Item item;
    item.storage_type = WebDragData::Item::kStorageTypeFilename;
    item.filename_data = blink::FilePathToWebString(it->path);
    item.display_name_data =
        blink::FilePathToWebString(base::FilePath(it->display_name));
    item_list.push_back(item);
  }

  for (auto it = drop_data.file_system_files.begin();
       it != drop_data.file_system_files.end(); ++it) {
    WebDragData::Item item;
    item.storage_type = WebDragData::Item::kStorageTypeFileSystemFile;
    item.file_system_url = it->url;
    item.file_system_file_size = it->size;
    item.file_system_id = blink::WebString::FromASCII(it->filesystem_id);
    item_list.push_back(item);
  }

  for (const auto& it : drop_data.custom_data) {
    WebDragData::Item item;
    item.storage_type = WebDragData::Item::kStorageTypeString;
    item.string_type = WebString::FromUTF16(it.first);
    item.string_data = WebString::FromUTF16(it.second);
    item_list.push_back(item);
  }

  WebDragData result;
  result.Initialize();
  result.SetItems(item_list);
  result.SetFilesystemId(WebString::FromUTF16(drop_data.filesystem_id));
  return result;
}

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

// Returns true if the device scale is high enough that losing subpixel
// antialiasing won't have a noticeable effect on text quality.
static bool DeviceScaleEnsuresTextQuality(float device_scale_factor) {
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // On Android, we never have subpixel antialiasing. On Chrome OS we prefer to
  // composite all scrollers so that we get animated overlay scrollbars.
  return true;
#else
  // 1.5 is a common touchscreen tablet device scale factor. For such
  // devices main thread antialiasing is a heavy burden.
  return device_scale_factor >= 1.5f;
#endif
}

static bool PreferCompositingToLCDText(CompositorDependencies* compositor_deps,
                                       float device_scale_factor) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisablePreferCompositingToLCDText))
    return false;
  if (command_line.HasSwitch(switches::kEnablePreferCompositingToLCDText))
    return true;
  if (!compositor_deps->IsLcdTextEnabled())
    return true;
  return DeviceScaleEnsuresTextQuality(device_scale_factor);
}

}  // namespace

// RenderWidget ---------------------------------------------------------------

// static
void RenderWidget::InstallCreateForFrameHook(
    CreateRenderWidgetFunction create_widget) {
  g_create_render_widget_for_frame = create_widget;
}

scoped_refptr<RenderWidget> RenderWidget::CreateForFrame(
    int32_t widget_routing_id,
    CompositorDependencies* compositor_deps,
    const ScreenInfo& screen_info,
    blink::WebDisplayMode display_mode,
    bool is_frozen,
    bool hidden,
    bool never_visible,
    mojom::WidgetRequest widget_request) {
  if (g_create_render_widget_for_frame) {
    return g_create_render_widget_for_frame(
        widget_routing_id, compositor_deps, screen_info, display_mode,
        is_frozen, hidden, never_visible, std::move(widget_request));
  }

  return base::WrapRefCounted(new RenderWidget(
      widget_routing_id, compositor_deps, screen_info, display_mode, is_frozen,
      hidden, never_visible, std::move(widget_request)));
}

scoped_refptr<RenderWidget> RenderWidget::CreateForPopup(
    int32_t widget_routing_id,
    CompositorDependencies* compositor_deps,
    const ScreenInfo& screen_info,
    blink::WebDisplayMode display_mode,
    bool is_frozen,
    bool hidden,
    bool never_visible,
    mojom::WidgetRequest widget_request) {
  return base::WrapRefCounted(new RenderWidget(
      widget_routing_id, compositor_deps, screen_info, display_mode, is_frozen,
      hidden, never_visible, std::move(widget_request)));
}

RenderWidget::RenderWidget(int32_t widget_routing_id,
                           CompositorDependencies* compositor_deps,
                           const ScreenInfo& screen_info,
                           blink::WebDisplayMode display_mode,
                           bool is_frozen,
                           bool hidden,
                           bool never_visible,
                           mojom::WidgetRequest widget_request)
    : routing_id_(widget_routing_id),
      compositor_deps_(compositor_deps),
      webwidget_internal_(nullptr),
      auto_resize_mode_(false),
      is_hidden_(hidden),
      // When there's no RenderThreadImpl we can't start the compositor without
      // crashing, so just behave like a never-visible widget.
      compositor_never_visible_(never_visible || !RenderThreadImpl::current()),
      is_fullscreen_granted_(false),
      display_mode_(display_mode),
      ime_event_guard_(nullptr),
      closing_(false),
      host_will_close_this_(false),
      is_frozen_(is_frozen),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      text_input_mode_(ui::TEXT_INPUT_MODE_DEFAULT),
      text_input_flags_(0),
      next_previous_flags_(kInvalidNextPreviousFlagsValue),
      can_compose_inline_(true),
      composition_range_(gfx::Range::InvalidRange()),
      pending_window_rect_count_(0),
      screen_info_(screen_info),
      monitor_composition_info_(false),
      popup_origin_scale_for_emulation_(0.f),
      frame_swap_message_queue_(new FrameSwapMessageQueue(routing_id_)),
      has_host_context_menu_location_(false),
      has_focus_(false),
      for_child_local_root_frame_(false),
#if defined(OS_MACOSX)
      text_input_client_observer_(new TextInputClientObserver(this)),
#endif
      first_update_visual_state_after_hidden_(false),
      was_shown_time_(base::TimeTicks::Now()),
      current_content_source_id_(0),
      widget_binding_(this, std::move(widget_request)),
      warmup_weak_ptr_factory_(this),
      weak_ptr_factory_(this) {
  DCHECK_NE(routing_id_, MSG_ROUTING_NONE);
  DCHECK(RenderThread::Get());

  // In tests there may not be a RenderThreadImpl.
  if (RenderThreadImpl::current()) {
    render_widget_scheduling_state_ = RenderThreadImpl::current()
                                          ->GetWebMainThreadScheduler()
                                          ->NewRenderWidgetSchedulingState();
    render_widget_scheduling_state_->SetHidden(is_hidden_);
  }
#if defined(USE_AURA)
  RendererWindowTreeClient::CreateIfNecessary(routing_id_);
  if (features::IsMultiProcessMash())
    RendererWindowTreeClient::Get(routing_id_)->SetVisible(!is_hidden_);
#endif

  if (routing_id_ != MSG_ROUTING_NONE)
    g_routing_id_widget_map.Get().emplace(routing_id_, this);
}

RenderWidget::~RenderWidget() {
  DCHECK(!webwidget_internal_) << "Leaking our WebWidget!";

  // TODO(ajwong): Add in check that routing_id_ has been removed from
  // g_routing_id_widget_map once the shutdown semantics for RenderWidget
  // and RenderViewImpl are rationalized. Currently, too many unit and
  // browser tests delete a RenderWidget without correclty going through
  // the shutdown. https://crbug.com/545684

#if defined(USE_AURA)
  // It is possible for a RenderWidget to be destroyed before it was embedded
  // in a mus window. The RendererWindowTreeClient will leak in such cases. So
  // explicitly delete it here.
  RendererWindowTreeClient::Destroy(routing_id_);
#endif
}

// static
RenderWidget* RenderWidget::FromRoutingID(int32_t routing_id) {
  RoutingIDWidgetMap* widgets = g_routing_id_widget_map.Pointer();
  auto it = widgets->find(routing_id);
  return it == widgets->end() ? NULL : it->second;
}

void RenderWidget::InitForPopup(ShowCallback show_callback,
                                blink::WebPagePopup* web_page_popup) {
  // Init() increments the reference count on |this|, making it
  // self-referencing.
  Init(std::move(show_callback), web_page_popup);
}

void RenderWidget::InitForChildLocalRoot(
    blink::WebFrameWidget* web_frame_widget) {
  for_child_local_root_frame_ = true;
  // Init() increments the reference count on |this|, making it
  // self-referencing.
  Init(base::NullCallback(), web_frame_widget);
}

void RenderWidget::CloseForFrame() {
  OnClose();
}

void RenderWidget::Init(ShowCallback show_callback, WebWidget* web_widget) {
  DCHECK(!webwidget_internal_);
  DCHECK_NE(routing_id_, MSG_ROUTING_NONE);

  RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();

  input_handler_ = std::make_unique<RenderWidgetInputHandler>(this, this);

  LayerTreeView* layer_tree_view = InitializeLayerTreeView();
  web_widget->SetLayerTreeView(layer_tree_view,
                               layer_tree_view->animation_host());

  blink::scheduler::WebThreadScheduler* main_thread_scheduler = nullptr;
  if (render_thread_impl)
    main_thread_scheduler = render_thread_impl->GetWebMainThreadScheduler();
  blink::scheduler::WebThreadScheduler* compositor_thread_scheduler =
      blink::scheduler::WebThreadScheduler::CompositorThreadScheduler();
  scoped_refptr<base::SingleThreadTaskRunner> compositor_input_task_runner;
  // Use the compositor thread task runner unless this is a popup or other such
  // non-frame widgets. The |compositor_thread_scheduler| can be null in tests
  // without a compositor thread.
  if (for_frame() && compositor_thread_scheduler) {
    compositor_input_task_runner =
        compositor_thread_scheduler->InputTaskRunner();
  }

  widget_input_handler_manager_ = WidgetInputHandlerManager::Create(
      weak_ptr_factory_.GetWeakPtr(), std::move(compositor_input_task_runner),
      main_thread_scheduler);

  show_callback_ = std::move(show_callback);

  webwidget_internal_ = web_widget;
  webwidget_mouse_lock_target_.reset(
      new WebWidgetLockTarget(webwidget_internal_));
  mouse_lock_dispatcher_.reset(new RenderWidgetMouseLockDispatcher(this));

  RenderThread::Get()->AddRoute(routing_id_, this);
  // Take a reference on behalf of the RenderThread.  This will be balanced
  // when we receive WidgetMsg_Close.
  AddRef();
}

void RenderWidget::ApplyEmulatedScreenMetricsForPopupWidget(
    RenderWidget* origin_widget) {
  RenderWidgetScreenMetricsEmulator* emulator =
      origin_widget->screen_metrics_emulator_.get();
  if (!emulator)
    return;
  popup_origin_scale_for_emulation_ = emulator->scale();
  popup_view_origin_for_emulation_ = emulator->applied_widget_rect().origin();
  popup_screen_origin_for_emulation_ =
      emulator->original_screen_rect().origin();
  UpdateSurfaceAndScreenInfo(local_surface_id_allocation_from_parent_,
                             CompositorViewportSize(),
                             emulator->original_screen_info());
}

gfx::Rect RenderWidget::AdjustValidationMessageAnchor(const gfx::Rect& anchor) {
  if (screen_metrics_emulator_)
    return screen_metrics_emulator_->AdjustValidationMessageAnchor(anchor);
  return anchor;
}

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
void RenderWidget::SetExternalPopupOriginAdjustmentsForEmulation(
    ExternalPopupMenu* popup) {
  if (screen_metrics_emulator_)
    popup->SetOriginScaleForEmulation(screen_metrics_emulator_->scale());
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
    IPC_MESSAGE_HANDLER(WidgetMsg_ShowContextMenu, OnShowContextMenu)
    IPC_MESSAGE_HANDLER(WidgetMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(WidgetMsg_SynchronizeVisualProperties,
                        OnSynchronizeVisualProperties)
    IPC_MESSAGE_HANDLER(WidgetMsg_EnableDeviceEmulation,
                        OnEnableDeviceEmulation)
    IPC_MESSAGE_HANDLER(WidgetMsg_DisableDeviceEmulation,
                        OnDisableDeviceEmulation)
    IPC_MESSAGE_HANDLER(WidgetMsg_WasHidden, OnWasHidden)
    IPC_MESSAGE_HANDLER(WidgetMsg_WasShown, OnWasShown)
    IPC_MESSAGE_HANDLER(WidgetMsg_SetActive, OnSetActive)
    IPC_MESSAGE_HANDLER(WidgetMsg_SetTextDirection, OnSetTextDirection)
    IPC_MESSAGE_HANDLER(WidgetMsg_SetBounds_ACK, OnRequestSetBoundsAck)
    IPC_MESSAGE_HANDLER(WidgetMsg_UpdateScreenRects, OnUpdateScreenRects)
    IPC_MESSAGE_HANDLER(WidgetMsg_ForceRedraw, OnForceRedraw)
    IPC_MESSAGE_HANDLER(WidgetMsg_SetViewportIntersection,
                        OnSetViewportIntersection)
    IPC_MESSAGE_HANDLER(WidgetMsg_SetIsInert, OnSetIsInert)
    IPC_MESSAGE_HANDLER(WidgetMsg_SetInheritedEffectiveTouchAction,
                        OnSetInheritedEffectiveTouchAction)
    IPC_MESSAGE_HANDLER(WidgetMsg_UpdateRenderThrottlingStatus,
                        OnUpdateRenderThrottlingStatus)
    IPC_MESSAGE_HANDLER(WidgetMsg_WaitForNextFrameForTests,
                        OnWaitNextFrameForTests)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragEnter, OnDragTargetDragEnter)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragOver, OnDragTargetDragOver)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragLeave, OnDragTargetDragLeave)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDrop, OnDragTargetDrop)
    IPC_MESSAGE_HANDLER(DragMsg_SourceEnded, OnDragSourceEnded)
    IPC_MESSAGE_HANDLER(DragMsg_SourceSystemDragEnded,
                        OnDragSourceSystemDragEnded)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool RenderWidget::Send(IPC::Message* message) {
  // Don't send any messages after the browser has told us to close, and filter
  // most outgoing messages when frozen.
  if (closing_) {
    delete message;
    return false;
  }
  if (is_frozen_ && !SwappedOutMessages::CanSendWhileSwappedOut(message)) {
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
  if (delegate())
    return has_focus_;
  if (for_child_local_root_frame_) {
    // TODO(ekaramad): We track page focus in all RenderViews on the page but
    // the RenderWidgets corresponding to child local roots do not get the
    // update. For now, this method returns true when the RenderWidget is for a
    // child local frame, i.e., IME events will be processed regardless of page
    // focus. We should revisit this after page focus for OOPIFs has been fully
    // resolved (https://crbug.com/689777).
    return true;
  }
  // Not a frame widget.
  return false;
}

void RenderWidget::OnClose() {
  DCHECK(content::RenderThread::Get());
  if (closing_)
    return;
  for (auto& observer : render_frames_)
    observer.WidgetWillClose();
  closing_ = true;

  // Browser correspondence is no longer needed at this point.
  if (routing_id_ != MSG_ROUTING_NONE) {
    RenderThread::Get()->RemoveRoute(routing_id_);
    g_routing_id_widget_map.Get().erase(routing_id_);
  }

  // Stop handling main thread input events immediately so we don't have them
  // running while things are partly shut down.
  if (input_event_queue_)
    input_event_queue_->ClearClient();

  if (for_child_local_root_frame_) {
    // Widgets for frames may be created and closed at any time while the frame
    // is alive. However, WebWidget must be closed synchronously because frame
    // widgets and frames hold pointers to each other. The deferred call to
    // Close() will complete cleanup and release |this|, but CloseWebWidget()
    // prevents Close() from attempting to access members of an
    // already-deleted frame.
    CloseWebWidget();
  }
  // If there is a Send call on the stack, then it could be dangerous to close
  // now.  Post a task that only gets invoked when there are no nested message
  // loops.
  //
  // The asynchronous Close() takes an owning reference to |this| keeping the
  // object alive beyond the Release() below. It is the last reference to this
  // object.
  //
  // TODO(https://crbug.com/545684): The actual lifetime for RenderWidget
  // seems to be single-owner. It is either owned by "IPC" events (popup,
  // mainframe, and fullscreen), or a RenderFrame. If Close() self-deleting,
  // all the ref-counting mess could be removed.
  GetCleanupTaskRunner()->PostNonNestableTask(
      FROM_HERE, base::BindOnce(&RenderWidget::Close, this));

  // Balances the AddRef taken when we called AddRoute.
  Release();
}

void RenderWidget::OnSynchronizeVisualProperties(
    const VisualProperties& original_params) {
  TRACE_EVENT0("renderer", "RenderWidget::OnSynchronizeVisualProperties");

  VisualProperties params = original_params;
  // Web tests can override the device scale factor in the renderer.
  if (device_scale_factor_for_testing_) {
    params.screen_info.device_scale_factor = device_scale_factor_for_testing_;
    params.compositor_viewport_pixel_size = gfx::ScaleToCeiledSize(
        params.new_size, params.screen_info.device_scale_factor);
  }

  // Inform the rendering thread of the color space indicating the presence of
  // HDR capabilities. The HDR bit happens to be globally true/false for all
  // browser windows (on Windows OS) and thus would be the same for all
  // RenderWidgets, so clobbering each other works out since only the HDR bit is
  // used. See https://crbug.com/803451 and
  // https://chromium-review.googlesource.com/c/chromium/src/+/852912/15#message-68bbd3e25c3b421a79cd028b2533629527d21fee
  //
  // The RenderThreadImpl can be null in tests.
  {
    RenderThreadImpl* render_thread = RenderThreadImpl::current();
    if (render_thread)
      render_thread->SetRenderingColorSpace(params.screen_info.color_space);
  }

  if (delegate()) {
    if (size_ != params.new_size) {
      // Only hide popups when the size changes. Eg https://crbug.com/761908.
      delegate()->CancelPagePopupForWidget();
    }

    if (display_mode_ != params.display_mode) {
      display_mode_ = params.display_mode;
      delegate()->ApplyNewDisplayModeForWidget(params.display_mode);
    }

    bool auto_resize_mode_changed =
        auto_resize_mode_ != params.auto_resize_enabled;
    auto_resize_mode_ = params.auto_resize_enabled;
    min_size_for_auto_resize_ = params.min_size_for_auto_resize;
    max_size_for_auto_resize_ = params.max_size_for_auto_resize;

    if (auto_resize_mode_) {
      gfx::Size min_auto_size = min_size_for_auto_resize_;
      gfx::Size max_auto_size = max_size_for_auto_resize_;
      if (compositor_deps_->IsUseZoomForDSFEnabled()) {
        min_auto_size = gfx::ScaleToCeiledSize(
            min_auto_size, params.screen_info.device_scale_factor);
        max_auto_size = gfx::ScaleToCeiledSize(
            max_auto_size, params.screen_info.device_scale_factor);
      }
      delegate()->ApplyAutoResizeLimitsForWidget(min_auto_size, max_auto_size);
    } else if (auto_resize_mode_changed) {
      delegate()->DisableAutoResizeForWidget();
      if (params.new_size.IsEmpty())
        return;
    }

    browser_controls_shrink_blink_size_ =
        params.browser_controls_shrink_blink_size;
    top_controls_height_ = params.top_controls_height;
    bottom_controls_height_ = params.bottom_controls_height;
  }

  bool ignore_resize_ipc = false;
  if (synchronous_resize_mode_for_testing_) {
    // We can ignore browser-initialized resizing during synchronous
    // (renderer-controlled) mode, unless it is switching us to/from
    // fullsreen mode or changing the device scale factor.
    // TODO(danakj): Does the browser actually change DSF inside a web test??
    // TODO(danakj): Isn't the display mode check redundant with the fullscreen
    // one?
    if (params.is_fullscreen_granted == is_fullscreen_granted_ &&
        params.display_mode == display_mode_ &&
        params.screen_info.device_scale_factor ==
            screen_info_.device_scale_factor)
      ignore_resize_ipc = true;
  }

  // When controlling the size in the renderer, we should ignore sizes given by
  // the browser IPC here.
  // TODO(danakj): There are many things also being ignored that aren't the
  // widget's size params. It works because tests that use this mode don't
  // change those parameters, I guess. But it's more complicated then because it
  // looks like they are related to sync resize mode. Let's move them out of
  // this block.
  // TODO(danakj): It would be nice if we can still use the emulator to emulate
  // things other than the size if we are in sync resize mode - if the emulator
  // is even used in sync resize tests. It probably isn't though, so either way
  // it'd be good to get the emulator out of this block (maybe by overwriting
  // some of |params| in sync resize mode instead of just skipping the emulator.
  if (!ignore_resize_ipc) {
    if (screen_metrics_emulator_) {
      // This will call our SynchronizeVisualProperties() method with a
      // different set of VisualProperties, holding emulated values. Though not
      // all VisualProperties are modified by the metrics emulator, so it's a
      // bit unclear to do this with the full structure. Anything it does not
      // modify can be consumed directly here instead of in
      // SynchronizeVisualProperties().
      screen_metrics_emulator_->OnSynchronizeVisualProperties(params);
    } else {
      if (!delegate()) {
        // The main frame controls the page scale factor, from blink. For other
        // frame widgets, the page scale is received from its parent as part of
        // the visual properties here. While blink doesn't need to know this
        // page scale factor outside the main frame, the compositor does in
        // order to produce its output at the correct scale.
        layer_tree_view_->SetExternalPageScaleFactor(params.page_scale_factor);
        // Store the value to give to any new RenderFrameProxy that is
        // registered.
        page_scale_factor_from_mainframe_ = params.page_scale_factor;
        // Push the page scale factor down to any child RenderWidgets via our
        // child proxy frames.
        // TODO(danakj): This ends up setting the page scale factor in the
        // RenderWidgetHost of the child RenderWidget, so that it can bounce
        // the value down to its RenderWidget. Since this is essentially a
        // global value per-page, we could instead store it once in the browser
        // (such as in RenderViewHost) and distribute it to each frame-hosted
        // RenderWidget from there.
        for (auto& child_proxy : render_frame_proxies_)
          child_proxy.OnPageScaleFactorChanged(params.page_scale_factor);
      }

      gfx::Size old_visible_viewport_size = visible_viewport_size_;
      SynchronizeVisualProperties(params);
      if (old_visible_viewport_size != visible_viewport_size_) {
        for (auto& render_frame : render_frames_)
          render_frame.DidChangeVisibleViewport();
      }
    }
  }

  if (delegate() && params.scroll_focused_node_into_view)
    delegate()->ScrollFocusedNodeIntoViewForWidget();
}

void RenderWidget::OnEnableDeviceEmulation(
    const blink::WebDeviceEmulationParams& params) {
  if (!screen_metrics_emulator_) {
    VisualProperties visual_properties;
    visual_properties.screen_info = screen_info_;
    visual_properties.new_size = size_;
    visual_properties.compositor_viewport_pixel_size = CompositorViewportSize();
    visual_properties.local_surface_id_allocation =
        local_surface_id_allocation_from_parent_;
    visual_properties.visible_viewport_size = visible_viewport_size_;
    visual_properties.is_fullscreen_granted = is_fullscreen_granted_;
    visual_properties.display_mode = display_mode_;
    screen_metrics_emulator_.reset(new RenderWidgetScreenMetricsEmulator(
        this, params, visual_properties, widget_screen_rect_,
        window_screen_rect_));
    screen_metrics_emulator_->Apply();
  } else {
    screen_metrics_emulator_->ChangeEmulationParams(params);
  }
}

void RenderWidget::OnDisableDeviceEmulation() {
  screen_metrics_emulator_.reset();
}

void RenderWidget::OnWasHidden() {
  // A frozen main frame widget will never be hidden since that would require it
  // to be shown first. It must be thawed before changing visibility.
  DCHECK(!is_frozen_);

  TRACE_EVENT0("renderer", "RenderWidget::OnWasHidden");

  SetHidden(true);

  for (auto& observer : render_frames_)
    observer.WasHidden();
}

void RenderWidget::OnWasShown(base::TimeTicks show_request_timestamp,
                              bool was_evicted) {
  // A frozen main frame widget does not become shown, since it has no frame
  // associated with it. It must be thawed before changing visibility.
  DCHECK(!is_frozen_);

  TRACE_EVENT_WITH_FLOW0("renderer", "RenderWidget::OnWasShown", routing_id(),
                         TRACE_EVENT_FLAG_FLOW_IN);

  was_shown_time_ = base::TimeTicks::Now();

  SetHidden(false);
  if (!show_request_timestamp.is_null()) {
    layer_tree_view_->layer_tree_host()->RequestPresentationTimeForNextFrame(
        CreateTabSwitchingTimeRecorder(show_request_timestamp));
  }

  for (auto& observer : render_frames_)
    observer.WasShown();
  if (was_evicted) {
    for (auto& observer : render_frame_proxies_)
      observer.WasEvicted();
  }
}

void RenderWidget::OnRequestSetBoundsAck() {
  DCHECK(pending_window_rect_count_);
  pending_window_rect_count_--;
}

void RenderWidget::OnForceRedraw(int snapshot_id) {
  RequestPresentation(base::BindOnce(&RenderWidget::DidPresentForceDrawFrame,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     snapshot_id));
}

void RenderWidget::RequestPresentation(PresentationTimeCallback callback) {
  layer_tree_view_->layer_tree_host()->RequestPresentationTimeForNextFrame(
      std::move(callback));
  layer_tree_view_->SetNeedsForcedRedraw();

  // Need this since single thread mode doesn't have a scheduler so the above
  // call won't cause us to generate a new frame.
  ScheduleAnimation();
}

void RenderWidget::DidPresentForceDrawFrame(
    int snapshot_id,
    const gfx::PresentationFeedback& feedback) {
  Send(new WidgetHostMsg_ForceRedrawComplete(routing_id(), snapshot_id));
}

viz::FrameSinkId RenderWidget::GetFrameSinkIdAtPoint(const gfx::PointF& point,
                                                     gfx::PointF* local_point) {
  return input_handler_->GetFrameSinkIdAtPoint(point, local_point);
}

bool RenderWidget::HandleInputEvent(
    const blink::WebCoalescedInputEvent& input_event,
    const ui::LatencyInfo& latency_info,
    HandledEventCallback callback) {
  if (is_frozen_)
    return false;
  input_handler_->HandleInputEvent(input_event, latency_info,
                                   std::move(callback));
  return true;
}

void RenderWidget::SetNeedsMainFrame() {
  // The WebWidgetClient is not |this| if tests override it for the WebView and
  // WebViewClient.
  blink::WebWidgetClient* client =
      delegate() ? delegate()->GetWebWidgetClientForWidget() : this;
  client->ScheduleAnimation();
}

scoped_refptr<MainThreadEventQueue> RenderWidget::GetInputEventQueue() {
  return input_event_queue_;
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

void RenderWidget::OnSetActive(bool active) {
  if (delegate())
    delegate()->SetActiveForWidget(active);
}

void RenderWidget::OnSetFocus(bool enable) {
  if (delegate())
    delegate()->DidReceiveSetFocusEventForWidget();
  SetFocus(enable);
}

void RenderWidget::SetFocus(bool enable) {
  has_focus_ = enable;

  if (GetWebWidget())
    GetWebWidget()->SetFocus(enable);

  for (auto& observer : render_frames_)
    observer.RenderWidgetSetFocus(enable);

  if (delegate())
    delegate()->DidChangeFocusForWidget();
}

///////////////////////////////////////////////////////////////////////////////
// LayerTreeViewDelegate

void RenderWidget::ApplyViewportChanges(
    const cc::ApplyViewportChangesArgs& args) {
  if (!GetWebWidget())
    return;
  GetWebWidget()->ApplyViewportChanges(args);
}

void RenderWidget::RecordWheelAndTouchScrollingCount(
    bool has_scrolled_by_wheel,
    bool has_scrolled_by_touch) {
  if (!GetWebWidget())
    return;
  GetWebWidget()->RecordWheelAndTouchScrollingCount(has_scrolled_by_wheel,
                                                    has_scrolled_by_touch);
}

void RenderWidget::SendOverscrollEventFromImplSide(
    const gfx::Vector2dF& overscroll_delta,
    cc::ElementId scroll_latched_element_id) {
  if (!GetWebWidget())
    return;
  GetWebWidget()->SendOverscrollEventFromImplSide(overscroll_delta,
                                                  scroll_latched_element_id);
}
void RenderWidget::SendScrollEndEventFromImplSide(
    cc::ElementId scroll_latched_element_id) {
  if (!GetWebWidget())
    return;
  GetWebWidget()->SendScrollEndEventFromImplSide(scroll_latched_element_id);
}

void RenderWidget::BeginMainFrame(base::TimeTicks frame_time) {
  if (!GetWebWidget())
    return;

  // We record metrics only when running in multi-threaded mode, not
  // single-thread mode for testing.
  bool record_main_frame_metrics =
      !!compositor_deps_->GetCompositorImplThreadTaskRunner();
  if (input_event_queue_) {
    base::Optional<ScopedUkmRafAlignedInputTimer> ukm_timer;
    if (record_main_frame_metrics)
      ukm_timer.emplace(GetWebWidget());
    input_event_queue_->DispatchRafAlignedInput(frame_time);
  }

  GetWebWidget()->BeginFrame(frame_time, record_main_frame_metrics);
}

void RenderWidget::DidBeginMainFrame() {
  if (!GetWebWidget())
    return;
  GetWebWidget()->DidBeginFrame();
}

void RenderWidget::RequestNewLayerTreeFrameSink(
    LayerTreeFrameSinkCallback callback) {
  // For widgets that are never visible, we don't start the compositor, so we
  // never get a request for a cc::LayerTreeFrameSink.
  DCHECK(!compositor_never_visible_);
  // Frozen RenderWidgets should not be doing any compositing.
  DCHECK(!is_frozen_);

  if (is_closing()) {
    // In this case, we drop the request which means the compositor waits
    // forever, which is fine since we're going to destroy it.
    return;
  }

  // If we have a warmup in progress, wait for that and store the callback
  // to be run when the warmup completes.
  if (warmup_frame_sink_request_pending_) {
    after_warmup_callback_ = std::move(callback);
    return;
  }
  // If a warmup previously completed, use the result.
  if (warmup_frame_sink_) {
    std::move(callback).Run(std::move(warmup_frame_sink_));
    return;
  }

  DoRequestNewLayerTreeFrameSink(std::move(callback));
}

void RenderWidget::DoRequestNewLayerTreeFrameSink(
    LayerTreeFrameSinkCallback callback) {
  // TODO(jonross): have this generated by the LayerTreeFrameSink itself, which
  // would then handle binding.
  mojom::RenderFrameMetadataObserverPtr ptr;
  mojom::RenderFrameMetadataObserverRequest request = mojo::MakeRequest(&ptr);
  mojom::RenderFrameMetadataObserverClientPtrInfo client_info;
  mojom::RenderFrameMetadataObserverClientRequest client_request =
      mojo::MakeRequest(&client_info);
  auto render_frame_metadata_observer =
      std::make_unique<RenderFrameMetadataObserverImpl>(std::move(request),
                                                        std::move(client_info));
  layer_tree_view_->SetRenderFrameObserver(
      std::move(render_frame_metadata_observer));
  GURL url = GetWebWidget()->GetURLForDebugTrace();
  // The |url| is not always available, fallback to a fixed string.
  if (url.is_empty())
    url = GURL("chrome://gpu/RenderWidget::RequestNewLayerTreeFrameSink");
  // TODO(danakj): This may not be accurate, depending on the intent. A child
  // local root could be in the same process as the view, so if the client is
  // meant to designate the process type, it seems kRenderer would be the
  // correct choice. If client is meant to designate the widget type, then
  // kOOPIF would denote that it is not for the main frame. However, kRenderer
  // would also be used for other widgets such as popups.
  const char* client_name = for_child_local_root_frame_ ? kOOPIF : kRenderer;
  RenderThreadImpl::current()->RequestNewLayerTreeFrameSink(
      routing_id_, frame_swap_message_queue_, std::move(url),
      std::move(callback), std::move(client_request), std::move(ptr),
      client_name);
}

void RenderWidget::DidCommitAndDrawCompositorFrame() {
  // NOTE: Tests may break if this event is renamed or moved. See
  // tab_capture_performancetest.cc.
  TRACE_EVENT0("gpu", "RenderWidget::DidCommitAndDrawCompositorFrame");

  for (auto& observer : render_frames_)
    observer.DidCommitAndDrawCompositorFrame();

  // Notify subclasses that we initiated the paint operation.
  DidInitiatePaint();

  Send(new WidgetHostMsg_DidCommitAndDrawCompositorFrame(routing_id_));
}

void RenderWidget::DidCommitCompositorFrame() {
  if (delegate())
    delegate()->DidCommitCompositorFrameForWidget();
}

void RenderWidget::DidCompletePageScaleAnimation() {
  if (delegate())
    delegate()->DidCompletePageScaleAnimationForWidget();
}

void RenderWidget::SetRootLayer(scoped_refptr<cc::Layer> layer) {
  layer_tree_view_->layer_tree_host()->SetRootLayer(std::move(layer));
}

void RenderWidget::ScheduleAnimation() {
  // This call is not needed in single thread mode for tests without a
  // scheduler, but they need to override the WebWidgetClient and replace this
  // method in order to schedule a synchronous composite task themselves.
  layer_tree_view_->SetNeedsBeginFrame();
}

void RenderWidget::SetShowFPSCounter(bool show) {
  cc::LayerTreeHost* host = layer_tree_view_->layer_tree_host();
  cc::LayerTreeDebugState debug_state = host->GetDebugState();
  debug_state.show_fps_counter = show;
  host->SetDebugState(debug_state);
}

void RenderWidget::SetShowPaintRects(bool show) {
  cc::LayerTreeHost* host = layer_tree_view_->layer_tree_host();
  cc::LayerTreeDebugState debug_state = host->GetDebugState();
  debug_state.show_paint_rects = show;
  host->SetDebugState(debug_state);
}

void RenderWidget::SetShowDebugBorders(bool show) {
  cc::LayerTreeHost* host = layer_tree_view_->layer_tree_host();
  cc::LayerTreeDebugState debug_state = host->GetDebugState();
  if (show)
    debug_state.show_debug_borders.set();
  else
    debug_state.show_debug_borders.reset();
  host->SetDebugState(debug_state);
}

void RenderWidget::SetShowScrollBottleneckRects(bool show) {
  cc::LayerTreeHost* host = layer_tree_view_->layer_tree_host();
  cc::LayerTreeDebugState debug_state = host->GetDebugState();
  debug_state.show_touch_event_handler_rects = show;
  debug_state.show_wheel_event_handler_rects = show;
  debug_state.show_non_fast_scrollable_rects = show;
  host->SetDebugState(debug_state);
}

void RenderWidget::SetShowHitTestBorders(bool show) {
  cc::LayerTreeHost* host = layer_tree_view_->layer_tree_host();
  cc::LayerTreeDebugState debug_state = host->GetDebugState();
  debug_state.show_hit_test_borders = show;
  host->SetDebugState(debug_state);
}

void RenderWidget::SetBackgroundColor(SkColor color) {
  layer_tree_view_->layer_tree_host()->set_background_color(color);
}

void RenderWidget::UpdateVisualState() {
  if (!GetWebWidget())
    return;

  // We record metrics only when running in multi-threaded mode, not
  // single-thread mode for testing.
  bool record_main_frame_metrics =
      !!compositor_deps_->GetCompositorImplThreadTaskRunner();

  // When recording main frame metrics set the lifecycle reason to
  // kBeginMainFrame, because this is the calller of UpdateLifecycle
  // for the main frame. Otherwise, set the reason to kTests, which is
  // the oinly other reason this method is called.
  WebWidget::LifecycleUpdateReason lifecycle_reason =
      record_main_frame_metrics
          ? WebWidget::LifecycleUpdateReason::kBeginMainFrame
          : WebWidget::LifecycleUpdateReason::kTest;
  GetWebWidget()->UpdateLifecycle(WebWidget::LifecycleUpdate::kAll,
                                  lifecycle_reason);
  GetWebWidget()->SetSuppressFrameRequestsWorkaroundFor704763Only(false);

  if (first_update_visual_state_after_hidden_) {
    RecordTimeToFirstActivePaint();
    first_update_visual_state_after_hidden_ = false;
  }
}

void RenderWidget::RecordTimeToFirstActivePaint() {
  RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
  base::TimeDelta sample = base::TimeTicks::Now() - was_shown_time_;
  if (render_thread_impl->NeedsToRecordFirstActivePaint(TTFAP_AFTER_PURGED)) {
    UMA_HISTOGRAM_TIMES("PurgeAndSuspend.Experimental.TimeToFirstActivePaint",
                        sample);
  }
  if (render_thread_impl->NeedsToRecordFirstActivePaint(
          TTFAP_5MIN_AFTER_BACKGROUNDED)) {
    UMA_HISTOGRAM_TIMES(
        "PurgeAndSuspend.Experimental.TimeToFirstActivePaint."
        "AfterBackgrounded.5min",
        sample);
  }
}

void RenderWidget::RecordStartOfFrameMetrics() {
  if (!GetWebWidget())
    return;

  GetWebWidget()->RecordStartOfFrameMetrics();
}

void RenderWidget::RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) {
  if (!GetWebWidget())
    return;

  GetWebWidget()->RecordEndOfFrameMetrics(frame_begin_time);
}

void RenderWidget::WillBeginCompositorFrame() {
  TRACE_EVENT0("gpu", "RenderWidget::willBeginCompositorFrame");

  if (!GetWebWidget())
    return;

  GetWebWidget()->SetSuppressFrameRequestsWorkaroundFor704763Only(true);

  // The UpdateTextInputState can result in further layout and possibly
  // enable GPU acceleration so they need to be called before any painting
  // is done.
  UpdateTextInputState();
  UpdateSelectionBounds();

  for (auto& observer : render_frame_proxies_)
    observer.WillBeginCompositorFrame();
}

std::unique_ptr<cc::SwapPromise> RenderWidget::RequestCopyOfOutputForWebTest(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  return RenderThreadImpl::current()->RequestCopyOfOutputForWebTest(
      routing_id_, std::move(request));
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetInputHandlerDelegate

void RenderWidget::FocusChangeComplete() {
  blink::WebFrameWidget* frame_widget = GetFrameWidget();
  if (!frame_widget)
    return;

  blink::WebLocalFrame* focused =
      frame_widget->LocalRoot()->View()->FocusedFrame();

  if (focused && focused->AutofillClient())
    focused->AutofillClient()->DidCompleteFocusChangeInFrame();
}

void RenderWidget::ObserveGestureEventAndResult(
    const blink::WebGestureEvent& gesture_event,
    const gfx::Vector2dF& unused_delta,
    const cc::OverscrollBehavior& overscroll_behavior,
    bool event_processed) {
  if (!compositor_deps_->IsElasticOverscrollEnabled())
    return;

  cc::InputHandlerScrollResult scroll_result;
  scroll_result.did_scroll = event_processed;
  scroll_result.did_overscroll_root = !unused_delta.IsZero();
  scroll_result.unused_scroll_delta = unused_delta;
  scroll_result.overscroll_behavior = overscroll_behavior;

  widget_input_handler_manager_->ObserveGestureEventOnMainThread(gesture_event,
                                                                 scroll_result);
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
  if (mojom::WidgetInputHandlerHost* host =
          widget_input_handler_manager_->GetWidgetInputHandlerHost()) {
    host->DidOverscroll(params);
  }
}

void RenderWidget::SetInputHandler(RenderWidgetInputHandler* input_handler) {
  // Nothing to do here. RenderWidget created the |input_handler| and will take
  // ownership of it. We just verify here that we don't already have an input
  // handler.
  DCHECK(!input_handler_);
}

void RenderWidget::ShowVirtualKeyboard() {
  // Blink can continue running and change input state between the Close IPC
  // and the task that actually closes this class.
  if (closing_)
    return;
  UpdateTextInputStateInternal(true, false);
}

void RenderWidget::ClearTextInputState() {
  text_input_info_ = blink::WebTextInputInfo();
  text_input_type_ = ui::TextInputType::TEXT_INPUT_TYPE_NONE;
  text_input_mode_ = ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT;
  can_compose_inline_ = false;
  text_input_flags_ = 0;
  next_previous_flags_ = kInvalidNextPreviousFlagsValue;
}

void RenderWidget::UpdateTextInputState() {
  // Blink can continue running and change input state between the Close IPC
  // and the task that actually closes this class.
  if (closing_)
    return;
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
#if defined(OS_ANDROID)
    if (next_previous_flags_ == kInvalidNextPreviousFlagsValue) {
      // Due to a focus change, values will be reset by the frame.
      // That case we only need fresh NEXT/PREVIOUS information.
      // Also we won't send WidgetHostMsg_TextInputStateChanged if next/previous
      // focusable status is changed.
      if (auto* controller = GetInputMethodController()) {
        next_previous_flags_ =
            controller->ComputeWebTextInputNextPreviousFlags();
      } else {
        // For safety in case GetInputMethodController() is null, because -1 is
        // invalid value to send to browser process.
        next_previous_flags_ = 0;
      }
    }
#else
    next_previous_flags_ = 0;
#endif
    params.flags |= next_previous_flags_;
    params.value = new_info.value.Utf16();
    params.selection_start = new_info.selection_start;
    params.selection_end = new_info.selection_end;
    params.composition_start = new_info.composition_start;
    params.composition_end = new_info.composition_end;
    params.can_compose_inline = new_can_compose_inline;
    // TODO(changwan): change instances of show_ime_if_needed to
    // show_virtual_keyboard.
    params.show_ime_if_needed = show_virtual_keyboard;
    params.reply_to_request = reply_to_request;
    Send(new WidgetHostMsg_TextInputStateChanged(routing_id(), params));

    text_input_info_ = new_info;
    text_input_type_ = new_type;
    text_input_mode_ = new_mode;
    can_compose_inline_ = new_can_compose_inline;
    text_input_flags_ = new_info.flags;

#if defined(OS_ANDROID)
    // If we send a new TextInputStateChanged message, we must also deliver a
    // new RenderFrameMetadata, as the IME will need this info to be updated.
    // TODO(ericrk): Consider folding the above IPC into RenderFrameMetadata.
    // https://crbug.com/912309
    if (IsSurfaceSynchronizationEnabled()) {
      layer_tree_view_->RequestForceSendMetadata();
    }
#endif
  }
}

bool RenderWidget::WillHandleGestureEvent(const blink::WebGestureEvent& event) {
  possible_drag_event_info_.event_source =
      ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH;
  possible_drag_event_info_.event_location =
      gfx::ToFlooredPoint(event.PositionInScreen());

  return false;
}

bool RenderWidget::WillHandleMouseEvent(const blink::WebMouseEvent& event) {
  for (auto& observer : render_frames_)
    observer.RenderWidgetWillHandleMouseEvent();

  possible_drag_event_info_.event_source =
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE;
  possible_drag_event_info_.event_location =
      gfx::Point(event.PositionInScreen().x, event.PositionInScreen().y);

  if (delegate())
    return delegate()->RenderWidgetWillHandleMouseEventForWidget(event);

  return false;
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetScreenMetricsDelegate

void RenderWidget::ResizeWebWidget() {
  gfx::Size size = GetSizeForWebWidget();
  if (delegate()) {
    delegate()->ResizeWebWidgetForWidget(size, top_controls_height_,
                                         bottom_controls_height_,
                                         browser_controls_shrink_blink_size_);
    return;
  }
  GetWebWidget()->Resize(size);
}

gfx::Size RenderWidget::GetSizeForWebWidget() const {
  if (compositor_deps_->IsUseZoomForDSFEnabled()) {
    return gfx::ScaleToCeiledSize(size_,
                                  GetOriginalScreenInfo().device_scale_factor);
  }

  return size_;
}

gfx::Size RenderWidget::CompositorViewportSize() const {
  return layer_tree_view_->layer_tree_host()->device_viewport_size();
}

void RenderWidget::UpdateZoom(double zoom_level) {
  blink::WebFrameWidget* frame_widget = GetFrameWidget();
  if (!frame_widget)
    return;
  RenderFrameImpl* render_frame =
      RenderFrameImpl::FromWebFrame(frame_widget->LocalRoot());

  // Return early if zoom level is unchanged.
  if (render_frame->GetZoomLevel() == zoom_level) {
    return;
  }

  render_frame->SetZoomLevel(zoom_level);

  for (auto& observer : render_frame_proxies_)
    observer.OnZoomLevelChanged(zoom_level);

  for (auto& plugin : browser_plugins_)
    plugin.OnZoomLevelChanged(zoom_level);
}

void RenderWidget::SynchronizeVisualProperties(const VisualProperties& params) {

  gfx::Size new_compositor_viewport_pixel_size =
      params.auto_resize_enabled
          ? gfx::ScaleToCeiledSize(size_,
                                   params.screen_info.device_scale_factor)
          : params.compositor_viewport_pixel_size;
  UpdateSurfaceAndScreenInfo(params.local_surface_id_allocation.value_or(
                                 viz::LocalSurfaceIdAllocation()),
                             new_compositor_viewport_pixel_size,
                             params.screen_info);
  UpdateCaptureSequenceNumber(params.capture_sequence_number);
  layer_tree_view_->SetBrowserControlsHeight(
      params.top_controls_height, params.bottom_controls_height,
      params.browser_controls_shrink_blink_size);

  UpdateZoom(params.zoom_level);

  if (!params.auto_resize_enabled) {
    visible_viewport_size_ = params.visible_viewport_size;
    display_mode_ = params.display_mode;
    size_ = params.new_size;

    ResizeWebWidget();

    gfx::Size visual_viewport_size = visible_viewport_size_;
    if (compositor_deps_->IsUseZoomForDSFEnabled()) {
      visual_viewport_size = gfx::ScaleToCeiledSize(
          visual_viewport_size, GetOriginalScreenInfo().device_scale_factor);
    }
    GetWebWidget()->ResizeVisualViewport(visual_viewport_size);

    // NOTE: We may have entered fullscreen mode without changing our size.
    SetIsFullscreen(params.is_fullscreen_granted);
  }
}

void RenderWidget::SetScreenMetricsEmulationParameters(
    bool enabled,
    const blink::WebDeviceEmulationParams& params) {
  // This is only supported in RenderView, which has an delegate().
  DCHECK(delegate());
  delegate()->SetScreenMetricsEmulationParametersForWidget(enabled, params);
}

void RenderWidget::SetScreenRects(const gfx::Rect& widget_screen_rect,
                                  const gfx::Rect& window_screen_rect) {
  widget_screen_rect_ = widget_screen_rect;
  window_screen_rect_ = window_screen_rect;
}

///////////////////////////////////////////////////////////////////////////////
// WebWidgetClient

void RenderWidget::IntrinsicSizingInfoChanged(
    const blink::WebIntrinsicSizingInfo& sizing_info) {
  Send(new WidgetHostMsg_IntrinsicSizingInfoChanged(routing_id_, sizing_info));
}

void RenderWidget::DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) {
  if (layout_type == blink::WebMeaningfulLayout::kVisuallyNonEmpty) {
    QueueMessage(new WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(routing_id_));
  }

  for (auto& observer : render_frames_)
    observer.DidMeaningfulLayout(layout_type);
}

// static
std::unique_ptr<cc::SwapPromise> RenderWidget::QueueMessageImpl(
    IPC::Message* msg,
    FrameSwapMessageQueue* frame_swap_message_queue,
    scoped_refptr<IPC::SyncMessageFilter> sync_message_filter,
    int source_frame_number) {
  bool first_message_for_frame = false;
  frame_swap_message_queue->QueueMessageForFrame(
      source_frame_number, base::WrapUnique(msg), &first_message_for_frame);
  if (first_message_for_frame) {
    std::unique_ptr<cc::SwapPromise> promise(new QueueMessageSwapPromise(
        sync_message_filter, frame_swap_message_queue, source_frame_number));
    return promise;
  }
  return nullptr;
}

void RenderWidget::SetHandlingInputEvent(bool handling_input_event) {
  input_handler_->set_handling_input_event(handling_input_event);
}

void RenderWidget::QueueMessage(IPC::Message* msg) {
  if (closing_)
    return;

  // RenderThreadImpl::current() is NULL in some tests.
  if (!RenderThreadImpl::current()) {
    Send(msg);
    return;
  }

  std::unique_ptr<cc::SwapPromise> swap_promise =
      QueueMessageImpl(msg, frame_swap_message_queue_.get(),
                       RenderThreadImpl::current()->sync_message_filter(),
                       layer_tree_view_->GetSourceFrameNumber());

  if (swap_promise)
    layer_tree_view_->QueueSwapPromise(std::move(swap_promise));
}

void RenderWidget::DidChangeCursor(const WebCursorInfo& cursor_info) {
  // TODO(darin): Eliminate this temporary.
  WebCursor cursor;
  InitializeCursorFromWebCursorInfo(&cursor, cursor_info);
  // Only send a SetCursor message if we need to make a change.
  if (input_handler_->DidChangeCursor(cursor))
    Send(new WidgetHostMsg_SetCursor(routing_id_, cursor));
}

void RenderWidget::AutoscrollStart(const blink::WebFloatPoint& point) {
  Send(new WidgetHostMsg_AutoscrollStart(routing_id_, point));
}

void RenderWidget::AutoscrollFling(const blink::WebFloatSize& velocity) {
  Send(new WidgetHostMsg_AutoscrollFling(routing_id_, velocity));
}

void RenderWidget::AutoscrollEnd() {
  Send(new WidgetHostMsg_AutoscrollEnd(routing_id_));
}

// We are supposed to get a single call to Show for a newly created RenderWidget
// that was created via RenderWidget::CreateWebView.  So, we wait until this
// point to dispatch the ShowWidget message.
//
// This method provides us with the information about how to display the newly
// created RenderWidget (i.e., as a blocked popup or as a new tab).
//
void RenderWidget::Show(WebNavigationPolicy policy) {
  if (!show_callback_) {
    if (delegate()) {
      // When SupportsMultipleWindows is disabled, popups are reusing
      // the view's RenderWidget. In some scenarios, this makes blink to call
      // Show() twice. But otherwise, if it is enabled, we should not visit
      // Show() more than once.
      DCHECK(!delegate()->SupportsMultipleWindowsForWidget());
      return;
    } else {
      NOTREACHED() << "received extraneous Show call";
    }
  }

  DCHECK(routing_id_ != MSG_ROUTING_NONE);

  // The opener is responsible for actually showing this widget.
  std::move(show_callback_).Run(this, policy, initial_rect_);

  // NOTE: initial_rect_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if as_popup is false, or the browser
  // process will impose a default position otherwise.
  SetPendingWindowRect(initial_rect_);
}

LayerTreeView* RenderWidget::InitializeLayerTreeView() {
  TRACE_EVENT0("blink", "RenderWidget::InitializeLayerTreeView");

  layer_tree_view_ = std::make_unique<LayerTreeView>(
      this, compositor_deps_->GetCompositorMainThreadTaskRunner(),
      compositor_deps_->GetCompositorImplThreadTaskRunner(),
      compositor_deps_->GetTaskGraphRunner(),
      compositor_deps_->GetWebMainThreadScheduler());
  layer_tree_view_->Initialize(
      GenerateLayerTreeSettings(compositor_deps_, for_child_local_root_frame_,
                                screen_info_.rect.size(),
                                screen_info_.device_scale_factor),
      compositor_deps_->CreateUkmRecorderFactory());

  UpdateSurfaceAndScreenInfo(local_surface_id_allocation_from_parent_,
                             CompositorViewportSize(), screen_info_);
  layer_tree_view_->SetContentSourceId(current_content_source_id_);
  // If the widget is hidden, delay starting the compositor until the user shows
  // it. Also if the RenderWidget is frozen, we delay starting the compositor
  // until we expect to use the widget, which will be signaled through
  // WarmupCompositor().
  if (!is_frozen_ && !is_hidden_)
    StartStopCompositor();

  DCHECK_NE(MSG_ROUTING_NONE, routing_id_);
  layer_tree_view_->SetFrameSinkId(
      viz::FrameSinkId(RenderThread::Get()->GetClientId(), routing_id_));

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  if (render_thread) {
    input_event_queue_ = base::MakeRefCounted<MainThreadEventQueue>(
        this, render_thread->GetWebMainThreadScheduler()->InputTaskRunner(),
        render_thread->GetWebMainThreadScheduler(),
        /*allow_raf_aligned_input=*/!compositor_never_visible_);
  }

  return layer_tree_view_.get();
}

void RenderWidget::StartStopCompositor() {
  if (compositor_never_visible_)
    return;

  if (is_frozen_) {
    layer_tree_view_->SetVisible(false);
    // Drop all gpu resources, this makes SetVisible(true) more expensive/slower
    // but we don't expect to use this RenderWidget again until some possible
    // future navigation. This brings us a bit closer to emulating deleting the
    // RenderWidget instead of just stopping the compositor.
    layer_tree_view_->ReleaseLayerTreeFrameSink();
  } else if (is_hidden_) {
    layer_tree_view_->SetVisible(false);
  } else {
    layer_tree_view_->SetVisible(true);
  }
}

void RenderWidget::SetIsFrozen(bool is_frozen) {
  DCHECK_NE(is_frozen, is_frozen_);
  is_frozen_ = is_frozen;
  // If hidden, then frozen changing doesn't change anything with the
  // compositor since when hidden the compositor is always stopped.
  if (!is_hidden_)
    StartStopCompositor();
}

void RenderWidget::WarmupCompositor() {
  DCHECK(is_frozen_);
  if (compositor_never_visible_)
    return;

  // Keeping things simple. This would cancel any outstanding warmup if we
  // happened to have one (this should be basically impossible). This avoids any
  // extra book keeping about the outstanding reqeust.
  warmup_weak_ptr_factory_.InvalidateWeakPtrs();
  // And if we already did a warmup then we're done.
  if (warmup_frame_sink_)
    return;

  // Mark us pending the warmup frame sink *before* calling
  // DoRequestNewLayerTreeFrameSink() as it may run the reply callback
  // synchronously. So we don't want to change any state after the call
  // to DoRequestNewLayerTreeFrameSink() here.
  warmup_frame_sink_request_pending_ = true;

  auto cb = base::BindOnce(&RenderWidget::OnReplyForWarmupCompositor,
                           warmup_weak_ptr_factory_.GetWeakPtr());
  DoRequestNewLayerTreeFrameSink(std::move(cb));
}

void RenderWidget::OnReplyForWarmupCompositor(
    std::unique_ptr<cc::LayerTreeFrameSink> sink) {
  warmup_frame_sink_request_pending_ = false;

  if (after_warmup_callback_)
    std::move(after_warmup_callback_).Run(std::move(sink));
  else
    warmup_frame_sink_ = std::move(sink);
}

void RenderWidget::AbortWarmupCompositor() {
  warmup_frame_sink_request_pending_ = false;
  // Drop any pending warmup.
  warmup_weak_ptr_factory_.InvalidateWeakPtrs();
  // And drop any completed one.
  warmup_frame_sink_.reset();

  // If we had saved a callback to run after warmup, just do so now indicating
  // failure.
  if (after_warmup_callback_)
    std::move(after_warmup_callback_).Run(nullptr);
}

void RenderWidget::DoDeferredClose() {
  Send(new WidgetHostMsg_Close(routing_id_));
}

void RenderWidget::CloseWidgetSoon() {
  DCHECK(content::RenderThread::Get());
  if (is_frozen_) {
    // This widget is currently not active. The active main frame widget is in a
    // different process.  Have the browser route the close request to the
    // active widget instead, so that the correct unload handlers are run.
    Send(new WidgetHostMsg_RouteCloseEvent(routing_id_));
    return;
  }

  // Prevent compositor from setting up new IPC channels, since we know a
  // WidgetMsg_Close is coming. We do this immediately, not in DoDeferredClose,
  // as the caller (eg WebPagePopupImpl) may start tearing down things after
  // calling this method, including detaching the frame from this RenderWidget.
  // Then trying to make a LayerTreeFrameSink would crash.
  // https://crbug.com/906340
  host_will_close_this_ = true;

  // If a page calls window.close() twice, we'll end up here twice, but that's
  // OK.  It is safe to send multiple Close messages.
  //
  // Ask the RenderWidgetHost to initiate close.  We could be called from deep
  // in Javascript.  If we ask the RendwerWidgetHost to close now, the window
  // could be closed before the JS finishes executing.  So instead, post a
  // message back to the message loop, which won't run until the JS is
  // complete, and then the Close message can be sent.
  GetCleanupTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&RenderWidget::DoDeferredClose, this));
}

void RenderWidget::Close() {
  // This was done immediately in the |for_child_local_root_frame_| case in the
  // OnClose() IPC handler.
  if (!for_child_local_root_frame_)
    CloseWebWidget();

  layer_tree_view_.reset();
  if (delegate())
    delegate()->DidCloseWidget();
  // Note the ACK is a control message going to the RenderProcessHost.
  RenderThread::Get()->Send(new WidgetHostMsg_Close_ACK(routing_id()));
}

void RenderWidget::CloseWebWidget() {
  // If the browser has not sent OnDisableDeviceEmulation, we have an emulator
  // hanging out still. Destroying it must happen *after* the IPC route is
  // removed so that another IPC does not arrive and re-create the emulator
  // during closing.
  //
  // This destruction is normally part of an IPC and expects objects to be alive
  // that would be alive while the IPC route is active such as the
  // |layer_tree_view_|. So we ensure that it is the first thing to be
  // destroyed here before deleting things from the RenderWidget or the
  // delegate().
  //
  // TODO(danakj): The emulator could reset to non-emulated values in an
  // explicit method call (instead of in the destructor) that occurs when
  // emulation is disabled, but does not need to occur during RenderWidget
  // closing. Then we would not have to destroy this so carefully.
  screen_metrics_emulator_.reset();

  // When delegate() is present, the RenderWidget is for a main frame,
  // and the GetWebWidget() will be a WebFrameWidget, which is not the same as
  // |webwidget_internal_|. The WebFrameWidget will be closed when the main
  // frame is detached, so we do not close it here. But it does not close the
  // |webwidget_internal_| since this class takes responsibility for that here
  // in all cases.
  webwidget_internal_->Close();
  webwidget_internal_ = nullptr;
}

void RenderWidget::UpdateWebViewWithDeviceScaleFactor() {
  blink::WebFrameWidget* frame_widget = GetFrameWidget();
  blink::WebFrame* current_frame =
      frame_widget ? frame_widget->LocalRoot() : nullptr;
  blink::WebView* webview = current_frame ? current_frame->View() : nullptr;
  if (webview) {
    if (compositor_deps_->IsUseZoomForDSFEnabled())
      webview->SetZoomFactorForDeviceScaleFactor(
          GetWebScreenInfo().device_scale_factor);
    else
      webview->SetDeviceScaleFactor(GetWebScreenInfo().device_scale_factor);

    webview->GetSettings()->SetPreferCompositingToLCDTextEnabled(
        PreferCompositingToLCDText(compositor_deps_,
                                   GetWebScreenInfo().device_scale_factor));
  }
}

blink::WebFrameWidget* RenderWidget::GetFrameWidget() const {
  // TODO(danakj): Remove this check and don't call this method for non-frames.
  if (!for_frame())
    return nullptr;
  // TODO(danakj): Is this needed? IPCs stop after closing, but code used to
  // check for a null WebWidget.
  if (closing_)
    return nullptr;

  blink::WebWidget* widget;
  if (delegate()) {
    // Main frame WebFrameWidgets are held by the delegate, the internal widget
    // points directly to the WebView.
    // TODO(ekaramad): We should drop IPCs when |is_frozen_| instead of
    // handling them and finding a null here. However there is also the case
    // of the frame being detached without the widget being frozen to be
    // resolved (https://crbug.com/906340). So for now this can return null.
    widget = delegate()->GetWebWidgetForWidget();
  } else {
    // Subframes always have a WebFrameWidget themselves.
    widget = webwidget_internal_;
  }
  return static_cast<blink::WebFrameWidget*>(widget);
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
  WebRect rect = widget_screen_rect_;
  ScreenRectToEmulatedIfNeeded(&rect);
  return rect;
}

void RenderWidget::SetToolTipText(const blink::WebString& text,
                                  WebTextDirection hint) {
  Send(new WidgetHostMsg_SetTooltipText(routing_id_, text.Utf16(), hint));
}

void RenderWidget::SetWindowRect(const WebRect& rect_in_screen) {
  // This path is for the renderer to change the on-screen position/size of
  // the widget by changing its window rect. This is not possible for
  // RenderWidgets whose position/size are controlled by layout from another
  // frame tree (ie. child local root frames), as the window rect can only be
  // set by the browser.
  if (for_child_local_root_frame_)
    return;

  WebRect window_rect = rect_in_screen;
  EmulatedToScreenRectIfNeeded(&window_rect);

  if (synchronous_resize_mode_for_testing_) {
    // This is a web-test-only path. At one point, it was planned to be
    // removed. See https://crbug.com/309760.
    SetWindowRectSynchronously(window_rect);
    return;
  }

  if (show_callback_) {
    // The widget is not shown yet. Delay the |window_rect| being sent to the
    // browser until Show() is called so it can be sent with that IPC, once the
    // browser is ready for the info.
    initial_rect_ = window_rect;
  } else {
    Send(new WidgetHostMsg_RequestSetBounds(routing_id_, window_rect));
    SetPendingWindowRect(window_rect);
  }
}

void RenderWidget::SetPendingWindowRect(const WebRect& rect) {
  pending_window_rect_ = rect;
  pending_window_rect_count_++;

  // Popups don't get size updates back from the browser so just store the set
  // values.
  if (!for_frame()) {
    window_screen_rect_ = rect;
    widget_screen_rect_ = rect;
  }
}

void RenderWidget::OnShowContextMenu(ui::MenuSourceType source_type,
                                     const gfx::Point& location) {
  has_host_context_menu_location_ = true;
  host_context_menu_location_ = location;
  if (GetWebWidget()) {
    GetWebWidget()->ShowContextMenu(
        static_cast<blink::WebMenuSourceType>(source_type));
  }
  has_host_context_menu_location_ = false;
}

void RenderWidget::OnImeSetComposition(
    const base::string16& text,
    const std::vector<WebImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int selection_start,
    int selection_end) {
  if (!ShouldHandleImeEvents())
    return;

#if BUILDFLAG(ENABLE_PLUGINS)
  if (auto* plugin = GetFocusedPepperPluginInsideWidget()) {
    plugin->render_frame()->OnImeSetComposition(text, ime_text_spans,
                                                selection_start, selection_end);
    return;
  }
#endif
  ImeEventGuard guard(this);
  blink::WebInputMethodController* controller = GetInputMethodController();
  if (!controller ||
      !controller->SetComposition(
          WebString::FromUTF16(text), WebVector<WebImeTextSpan>(ime_text_spans),
          replacement_range.IsValid()
              ? WebRange(replacement_range.start(), replacement_range.length())
              : WebRange(),
          selection_start, selection_end)) {
    // If we failed to set the composition text, then we need to let the browser
    // process to cancel the input method's ongoing composition session, to make
    // sure we are in a consistent state.
    if (mojom::WidgetInputHandlerHost* host =
            widget_input_handler_manager_->GetWidgetInputHandlerHost()) {
      host->ImeCancelComposition();
    }
  }
  UpdateCompositionInfo(false /* not an immediate request */);
}

void RenderWidget::OnImeCommitText(
    const base::string16& text,
    const std::vector<WebImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int relative_cursor_pos) {
  if (!ShouldHandleImeEvents())
    return;

#if BUILDFLAG(ENABLE_PLUGINS)
  if (auto* plugin = GetFocusedPepperPluginInsideWidget()) {
    plugin->render_frame()->OnImeCommitText(text, replacement_range,
                                            relative_cursor_pos);
    return;
  }
#endif
  ImeEventGuard guard(this);
  input_handler_->set_handling_input_event(true);
  if (auto* controller = GetInputMethodController()) {
    controller->CommitText(
        WebString::FromUTF16(text), WebVector<WebImeTextSpan>(ime_text_spans),
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
  if (auto* plugin = GetFocusedPepperPluginInsideWidget()) {
    plugin->render_frame()->OnImeFinishComposingText(keep_selection);
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

void RenderWidget::UpdateSurfaceAndScreenInfo(
    const viz::LocalSurfaceIdAllocation& new_local_surface_id_allocation,
    const gfx::Size& compositor_viewport_pixel_size,
    const ScreenInfo& new_screen_info) {
  bool orientation_changed =
      screen_info_.orientation_angle != new_screen_info.orientation_angle ||
      screen_info_.orientation_type != new_screen_info.orientation_type;
  bool web_device_scale_factor_changed =
      screen_info_.device_scale_factor != new_screen_info.device_scale_factor;
  ScreenInfo previous_original_screen_info = GetOriginalScreenInfo();

  local_surface_id_allocation_from_parent_ = new_local_surface_id_allocation;
  screen_info_ = new_screen_info;

  // Note carefully that the DSF specified in |new_screen_info| is not the
  // DSF used by the compositor during device emulation!
  layer_tree_view_->SetViewportSizeAndScale(
      compositor_viewport_pixel_size,
      GetOriginalScreenInfo().device_scale_factor,
      local_surface_id_allocation_from_parent_);
  // The ViewportVisibleRect derives from the LayerTreeView's viewport size,
  // which is set above.
  layer_tree_view_->SetViewportVisibleRect(ViewportVisibleRect());
  layer_tree_view_->SetRasterColorSpace(
      screen_info_.color_space.GetRasterColorSpace());

  if (orientation_changed)
    OnOrientationChange();

  if (previous_original_screen_info != GetOriginalScreenInfo()) {
    for (auto& observer : render_frame_proxies_)
      observer.OnScreenInfoChanged(GetOriginalScreenInfo());

    // Notify all embedded BrowserPlugins of the updated ScreenInfo.
    for (auto& observer : browser_plugins_)
      observer.ScreenInfoChanged(GetOriginalScreenInfo());
  }

  if (web_device_scale_factor_changed)
    UpdateWebViewWithDeviceScaleFactor();
}

void RenderWidget::SetWindowRectSynchronously(
    const gfx::Rect& new_window_rect) {
  VisualProperties visual_properties;
  visual_properties.screen_info = screen_info_;
  visual_properties.new_size = new_window_rect.size();
  visual_properties.compositor_viewport_pixel_size = gfx::ScaleToCeiledSize(
      new_window_rect.size(), GetWebScreenInfo().device_scale_factor);
  visual_properties.visible_viewport_size = new_window_rect.size();
  visual_properties.is_fullscreen_granted = is_fullscreen_granted_;
  visual_properties.display_mode = display_mode_;
  visual_properties.local_surface_id_allocation =
      local_surface_id_allocation_from_parent_;
  // We are resizing the window from the renderer, so allocate a new
  // viz::LocalSurfaceId to avoid surface invariants violations in tests.
  layer_tree_view_->RequestNewLocalSurfaceId();
  SynchronizeVisualProperties(visual_properties);

  widget_screen_rect_ = new_window_rect;
  window_screen_rect_ = new_window_rect;
  if (show_callback_) {
    // Tests may call here directly to control the window rect. If
    // Show() did not happen yet, the rect is stored to be passed to the
    // browser when the RenderWidget requests Show().
    initial_rect_ = new_window_rect;
  }
}

void RenderWidget::UpdateCaptureSequenceNumber(
    uint32_t capture_sequence_number) {
  if (capture_sequence_number == last_capture_sequence_number_)
    return;
  last_capture_sequence_number_ = capture_sequence_number;

  // Notify observers of the new capture sequence number.
  for (auto& observer : render_frame_proxies_)
    observer.UpdateCaptureSequenceNumber(capture_sequence_number);
  for (auto& observer : browser_plugins_)
    observer.UpdateCaptureSequenceNumber(capture_sequence_number);
}

void RenderWidget::OnSetTextDirection(WebTextDirection direction) {
  if (auto* frame = GetFocusedWebLocalFrameInWidget())
    frame->SetTextDirection(direction);
}

void RenderWidget::OnUpdateScreenRects(const gfx::Rect& widget_screen_rect,
                                       const gfx::Rect& window_screen_rect) {
  if (screen_metrics_emulator_) {
    screen_metrics_emulator_->OnUpdateScreenRects(widget_screen_rect,
                                                  window_screen_rect);
  } else {
    SetScreenRects(widget_screen_rect, window_screen_rect);
  }
  Send(new WidgetHostMsg_UpdateScreenRects_ACK(routing_id()));
}

void RenderWidget::OnSetViewportIntersection(
    const gfx::Rect& viewport_intersection,
    const gfx::Rect& compositor_visible_rect,
    blink::FrameOcclusionState occlusion_state) {
  if (auto* frame_widget = GetFrameWidget()) {
    compositor_visible_rect_ = compositor_visible_rect;
    frame_widget->SetRemoteViewportIntersection(viewport_intersection,
                                                occlusion_state);
    layer_tree_view_->SetViewportVisibleRect(ViewportVisibleRect());
  }
}

void RenderWidget::OnSetIsInert(bool inert) {
  if (auto* frame_widget = GetFrameWidget())
    frame_widget->SetIsInert(inert);
}

void RenderWidget::OnSetInheritedEffectiveTouchAction(
    cc::TouchAction touch_action) {
  if (auto* frame_widget = GetFrameWidget())
    frame_widget->SetInheritedEffectiveTouchAction(touch_action);
}

void RenderWidget::OnUpdateRenderThrottlingStatus(bool is_throttled,
                                                  bool subtree_throttled) {
  if (auto* frame_widget = GetFrameWidget())
    frame_widget->UpdateRenderThrottlingStatus(is_throttled, subtree_throttled);
}

void RenderWidget::OnDragTargetDragEnter(
    const std::vector<DropData::Metadata>& drop_meta_data,
    const gfx::PointF& client_point,
    const gfx::PointF& screen_point,
    WebDragOperationsMask ops,
    int key_modifiers) {
  blink::WebFrameWidget* frame_widget = GetFrameWidget();
  if (!frame_widget)
    return;

  WebDragOperation operation = frame_widget->DragTargetDragEnter(
      DropMetaDataToWebDragData(drop_meta_data), client_point, screen_point,
      ops, key_modifiers);

  Send(new DragHostMsg_UpdateDragCursor(routing_id(), operation));
}

void RenderWidget::OnDragTargetDragOver(const gfx::PointF& client_point,
                                        const gfx::PointF& screen_point,
                                        WebDragOperationsMask ops,
                                        int key_modifiers) {
  blink::WebFrameWidget* frame_widget = GetFrameWidget();
  if (!frame_widget)
    return;

  WebDragOperation operation = frame_widget->DragTargetDragOver(
      ConvertWindowPointToViewport(client_point), screen_point, ops,
      key_modifiers);

  Send(new DragHostMsg_UpdateDragCursor(routing_id(), operation));
}

void RenderWidget::OnDragTargetDragLeave(const gfx::PointF& client_point,
                                         const gfx::PointF& screen_point) {
  blink::WebFrameWidget* frame_widget = GetFrameWidget();
  if (!frame_widget)
    return;

  frame_widget
      ->DragTargetDragLeave(ConvertWindowPointToViewport(client_point),
                            screen_point);
}

void RenderWidget::OnDragTargetDrop(const DropData& drop_data,
                                    const gfx::PointF& client_point,
                                    const gfx::PointF& screen_point,
                                    int key_modifiers) {
  blink::WebFrameWidget* frame_widget = GetFrameWidget();
  if (!frame_widget)
    return;

  frame_widget->DragTargetDrop(DropDataToWebDragData(drop_data),
                               ConvertWindowPointToViewport(client_point),
                               screen_point, key_modifiers);
}

void RenderWidget::OnDragSourceEnded(const gfx::PointF& client_point,
                                     const gfx::PointF& screen_point,
                                     WebDragOperation op) {
  blink::WebFrameWidget* frame_widget = GetFrameWidget();
  if (!frame_widget)
    return;

  frame_widget->DragSourceEndedAt(ConvertWindowPointToViewport(client_point),
                                  screen_point, op);
}

void RenderWidget::OnDragSourceSystemDragEnded() {
  blink::WebFrameWidget* frame_widget = GetFrameWidget();
  if (!frame_widget)
    return;

  frame_widget->DragSourceSystemDragEnded();
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
  if (auto* plugin = GetFocusedPepperPluginInsideWidget())
    return plugin->text_input_type();
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
  if (mojom::WidgetInputHandlerHost* host =
          widget_input_handler_manager_->GetWidgetInputHandlerHost()) {
    host->ImeCompositionRangeChanged(composition_range_,
                                     composition_character_bounds_);
  }
}

void RenderWidget::ConvertViewportToWindow(blink::WebRect* rect) {
  if (compositor_deps_->IsUseZoomForDSFEnabled()) {
    float reverse = 1 / GetOriginalScreenInfo().device_scale_factor;
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
  if (compositor_deps_->IsUseZoomForDSFEnabled()) {
    rect->x *= GetOriginalScreenInfo().device_scale_factor;
    rect->y *= GetOriginalScreenInfo().device_scale_factor;
    rect->width *= GetOriginalScreenInfo().device_scale_factor;
    rect->height *= GetOriginalScreenInfo().device_scale_factor;
  }
}

void RenderWidget::OnRequestTextInputStateUpdate() {
#if defined(OS_ANDROID)
  // This task may run between the Close IPC and the task that actually closes
  // this class.
  if (closing_)
    return;
  DCHECK(!ime_event_guard_);
  UpdateSelectionBounds();
  UpdateTextInputStateInternal(false, true /* reply_to_request */);
#endif
}

void RenderWidget::OnRequestCompositionUpdates(bool immediate_request,
                                               bool monitor_updates) {
  monitor_composition_info_ = monitor_updates;
  if (!immediate_request)
    return;
  UpdateCompositionInfo(true /* immediate request */);
}

void RenderWidget::OnOrientationChange() {
  if (auto* frame_widget = GetFrameWidget()) {
    // LocalRoot() might return null for provisional main frames. In this case,
    // the frame hasn't committed a navigation and is not swapped into the tree
    // yet, so it doesn't make sense to send orientation change events to it.
    //
    // TODO(https://crbug.com/578349): This check should be cleaned up
    // once provisional frames are gone.
    if (frame_widget->LocalRoot())
      frame_widget->LocalRoot()->SendOrientationChangeEvent();
  }
}

void RenderWidget::SetHidden(bool hidden) {
  // A frozen main frame widget does not become shown or hidden, since it has
  // no frame associated with it. It must be thawed before changing visibility.
  DCHECK(!is_frozen_);

  if (is_hidden_ == hidden)
    return;

  // The status has changed.  Tell the RenderThread about it and ensure
  // throttled acks are released in case frame production ceases.
  is_hidden_ = hidden;

#if defined(USE_AURA)
  if (features::IsMultiProcessMash())
    RendererWindowTreeClient::Get(routing_id_)->SetVisible(!hidden);
#endif

  if (is_hidden_) {
    first_update_visual_state_after_hidden_ = true;
  }

  if (render_widget_scheduling_state_)
    render_widget_scheduling_state_->SetHidden(hidden);

  // If the renderer was hidden, resolve any pending synthetic gestures so they
  // aren't blocked waiting for a compositor frame to be generated.
  if (is_hidden_)
    widget_input_handler_manager_->InvokeInputProcessedCallback();

  StartStopCompositor();
}

void RenderWidget::SetIsFullscreen(bool fullscreen) {
  if (fullscreen == is_fullscreen_granted_)
    return;
  is_fullscreen_granted_ = fullscreen;
  if (is_fullscreen_granted_) {
    GetWebWidget()->DidEnterFullscreen();
  } else {
    GetWebWidget()->DidExitFullscreen();
  }
}

void RenderWidget::OnImeEventGuardStart(ImeEventGuard* guard) {
  if (!ime_event_guard_)
    ime_event_guard_ = guard;
}

void RenderWidget::OnImeEventGuardFinish(ImeEventGuard* guard) {
  if (ime_event_guard_ != guard)
    return;
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
  if (auto* plugin = GetFocusedPepperPluginInsideWidget()) {
    // TODO(kinaba) http://crbug.com/101101
    // Current Pepper IME API does not handle selection bounds. So we simply
    // use the caret position as an empty range for now. It will be updated
    // after Pepper API equips features related to surrounding text retrieval.
    blink::WebRect caret(plugin->GetCaretBounds());
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
    WidgetHostMsg_SelectionBounds_Params params;
    params.is_anchor_first = false;
    GetSelectionBounds(&params.anchor_rect, &params.focus_rect);
    if (selection_anchor_rect_ != params.anchor_rect ||
        selection_focus_rect_ != params.focus_rect) {
      selection_anchor_rect_ = params.anchor_rect;
      selection_focus_rect_ = params.focus_rect;
      if (auto* focused_frame = GetFocusedWebLocalFrameInWidget()) {
        focused_frame->SelectionTextDirection(params.focus_dir,
                                              params.anchor_dir);
        params.is_anchor_first = focused_frame->IsSelectionAnchorFirst();
      }
      Send(new WidgetHostMsg_SelectionBoundsChanged(routing_id_, params));
    }
  }

  UpdateCompositionInfo(false /* not an immediate request */);
}

void RenderWidget::DidAutoResize(const gfx::Size& new_size) {
  // Blink can continue running and do a layout/resize between the Close IPC
  // and the task that actually closes this class.
  if (closing_)
    return;

  WebRect new_size_in_window(0, 0, new_size.width(), new_size.height());
  ConvertViewportToWindow(&new_size_in_window);
  if (size_.width() != new_size_in_window.width ||
      size_.height() != new_size_in_window.height) {
    size_ = gfx::Size(new_size_in_window.width, new_size_in_window.height);

    if (synchronous_resize_mode_for_testing_) {
      gfx::Rect new_pos(WindowRect().x, WindowRect().y, size_.width(),
                        size_.height());
      widget_screen_rect_ = new_pos;
      window_screen_rect_ = new_pos;
    }

    // TODO(ccameron): Note that this destroys any information differentiating
    // |size_| from the compositor's viewport size. Also note that the
    // calculation of |new_compositor_viewport_pixel_size| does not appear to
    // take into account device emulation.
    layer_tree_view_->RequestNewLocalSurfaceId();
    gfx::Size new_compositor_viewport_pixel_size =
        gfx::ScaleToCeiledSize(size_, GetWebScreenInfo().device_scale_factor);
    UpdateSurfaceAndScreenInfo(local_surface_id_allocation_from_parent_,
                               new_compositor_viewport_pixel_size,
                               screen_info_);
  }
}

void RenderWidget::GetCompositionCharacterBounds(
    std::vector<gfx::Rect>* bounds) {
  DCHECK(bounds);
  bounds->clear();

#if BUILDFLAG(ENABLE_PLUGINS)
  if (GetFocusedPepperPluginInsideWidget())
    return;
#endif

  blink::WebInputMethodController* controller = GetInputMethodController();
  if (!controller)
    return;
  blink::WebVector<blink::WebRect> bounds_from_blink;
  if (!controller->GetCompositionCharacterBounds(bounds_from_blink))
    return;

  for (size_t i = 0; i < bounds_from_blink.size(); ++i) {
    ConvertViewportToWindow(&bounds_from_blink[i]);
    bounds->push_back(bounds_from_blink[i]);
  }
}

void RenderWidget::GetCompositionRange(gfx::Range* range) {
#if BUILDFLAG(ENABLE_PLUGINS)
  if (GetFocusedPepperPluginInsideWidget())
    return;
#endif
  blink::WebInputMethodController* controller = GetInputMethodController();
  WebRange web_range = controller ? controller->CompositionRange() : WebRange();
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
  if (auto* plugin = GetFocusedPepperPluginInsideWidget())
    return plugin->IsPluginAcceptingCompositionEvents();
#endif
  return true;
}

void RenderWidget::DidHandleGestureEvent(const WebGestureEvent& event,
                                         bool event_cancelled) {
  if (event_cancelled) {
    // The delegate() doesn't need to hear about cancelled events.
    return;
  }

#if defined(OS_ANDROID) || defined(USE_AURA)
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
  if (event.GetType() == blink::WebGestureEvent::kGestureTap) {
    // TODO(estade): hit test the event against focused node to make sure
    // the tap actually hit the focused node.
    blink::WebInputMethodController* controller = GetInputMethodController();
    blink::WebTextInputType text_input_type =
        controller ? controller->TextInputType() : blink::kWebTextInputTypeNone;

    Send(new WidgetHostMsg_FocusedNodeTouched(
        routing_id_, text_input_type != blink::kWebTextInputTypeNone));
  }
#endif
#endif

  // The delegate() gets to respond to handling gestures last.
  if (delegate())
    delegate()->DidHandleGestureEventForWidget(event);
}

void RenderWidget::DidOverscroll(
    const blink::WebFloatSize& overscrollDelta,
    const blink::WebFloatSize& accumulatedOverscroll,
    const blink::WebFloatPoint& position,
    const blink::WebFloatSize& velocity,
    const cc::OverscrollBehavior& behavior) {
#if defined(OS_MACOSX)
  // On OSX the user can disable the elastic overscroll effect. If that's the
  // case, don't forward the overscroll notification.
  DCHECK(compositor_deps());
  if (!compositor_deps()->IsElasticOverscrollEnabled())
    return;
#endif
  input_handler_->DidOverscrollFromBlink(overscrollDelta, accumulatedOverscroll,
                                         position, velocity, behavior);
}

// static
cc::LayerTreeSettings RenderWidget::GenerateLayerTreeSettings(
    CompositorDependencies* compositor_deps,
    bool is_for_subframe,
    const gfx::Size& initial_screen_size,
    float initial_device_scale_factor) {
  const bool is_threaded =
      !!compositor_deps->GetCompositorImplThreadTaskRunner();

  const base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  cc::LayerTreeSettings settings;

  settings.resource_settings.use_r16_texture =
      base::FeatureList::IsEnabled(media::kUseR16Texture);

  settings.commit_to_active_tree = !is_threaded;
  settings.is_layer_tree_for_subframe = is_for_subframe;

  // For web contents, layer transforms should scale up the contents of layers
  // to keep content always crisp when possible.
  settings.layer_transforms_should_scale_layer_contents = true;

  settings.main_frame_before_activation_enabled =
      cmd.HasSwitch(cc::switches::kEnableMainFrameBeforeActivation);

  // Checkerimaging is not supported for synchronous single-threaded mode, which
  // is what the renderer uses if its not threaded.
  settings.enable_checker_imaging =
      !cmd.HasSwitch(cc::switches::kDisableCheckerImaging) && is_threaded;

#if defined(OS_ANDROID)
  // We can use a more aggressive limit on Android since decodes tend to take
  // longer on these devices.
  settings.min_image_bytes_to_checker = 512 * 1024;  // 512kB

  // Re-rasterization of checker-imaged content with software raster can be too
  // costly on Android.
  settings.only_checker_images_with_gpu_raster = true;
#endif

  auto switch_value_as_int = [](const base::CommandLine& command_line,
                                const std::string& switch_string, int min_value,
                                int max_value, int* result) {
    std::string string_value = command_line.GetSwitchValueASCII(switch_string);
    int int_value;
    if (base::StringToInt(string_value, &int_value) && int_value >= min_value &&
        int_value <= max_value) {
      *result = int_value;
      return true;
    } else {
      LOG(WARNING) << "Failed to parse switch " << switch_string << ": "
                   << string_value;
      return false;
    }
  };

  int default_tile_size = 256;
#if defined(OS_ANDROID)
  const gfx::Size screen_size =
      gfx::ScaleToFlooredSize(initial_screen_size, initial_device_scale_factor);
  int display_width = screen_size.width();
  int display_height = screen_size.height();
  int numTiles = (display_width * display_height) / (256 * 256);
  if (numTiles > 16)
    default_tile_size = 384;
  if (numTiles >= 40)
    default_tile_size = 512;

  // Adjust for some resolutions that barely straddle an extra
  // tile when in portrait mode. This helps worst case scroll/raster
  // by not needing a full extra tile for each row.
  constexpr int tolerance = 10;  // To avoid rounding errors.
  int portrait_width = std::min(display_width, display_height);
  if (default_tile_size == 256 && std::abs(portrait_width - 768) < tolerance)
    default_tile_size += 32;
  if (default_tile_size == 384 && std::abs(portrait_width - 1200) < tolerance)
    default_tile_size += 32;
#elif defined(OS_CHROMEOS) || defined(OS_MACOSX)
  // Use 512 for high DPI (dsf=2.0f) devices.
  if (initial_device_scale_factor >= 2.0f)
    default_tile_size = 512;
#endif

  // TODO(danakj): This should not be a setting O_O; it should change when the
  // device scale factor on LayerTreeHost changes.
  settings.default_tile_size = gfx::Size(default_tile_size, default_tile_size);
  if (cmd.HasSwitch(switches::kDefaultTileWidth)) {
    int tile_width = 0;
    switch_value_as_int(cmd, switches::kDefaultTileWidth, 1,
                        std::numeric_limits<int>::max(), &tile_width);
    settings.default_tile_size.set_width(tile_width);
  }
  if (cmd.HasSwitch(switches::kDefaultTileHeight)) {
    int tile_height = 0;
    switch_value_as_int(cmd, switches::kDefaultTileHeight, 1,
                        std::numeric_limits<int>::max(), &tile_height);
    settings.default_tile_size.set_height(tile_height);
  }

  int max_untiled_layer_width = settings.max_untiled_layer_size.width();
  if (cmd.HasSwitch(switches::kMaxUntiledLayerWidth)) {
    switch_value_as_int(cmd, switches::kMaxUntiledLayerWidth, 1,
                        std::numeric_limits<int>::max(),
                        &max_untiled_layer_width);
  }
  int max_untiled_layer_height = settings.max_untiled_layer_size.height();
  if (cmd.HasSwitch(switches::kMaxUntiledLayerHeight)) {
    switch_value_as_int(cmd, switches::kMaxUntiledLayerHeight, 1,
                        std::numeric_limits<int>::max(),
                        &max_untiled_layer_height);
  }

  settings.max_untiled_layer_size =
      gfx::Size(max_untiled_layer_width, max_untiled_layer_height);

  settings.gpu_rasterization_msaa_sample_count =
      compositor_deps->GetGpuRasterizationMSAASampleCount();
  settings.gpu_rasterization_forced =
      compositor_deps->IsGpuRasterizationForced();

  settings.can_use_lcd_text = compositor_deps->IsLcdTextEnabled();
  settings.use_zero_copy = compositor_deps->IsZeroCopyEnabled();
  settings.use_partial_raster = compositor_deps->IsPartialRasterEnabled();
  settings.enable_elastic_overscroll =
      compositor_deps->IsElasticOverscrollEnabled();
  settings.resource_settings.use_gpu_memory_buffer_resources =
      compositor_deps->IsGpuMemoryBufferCompositorResourcesEnabled();
  settings.use_painted_device_scale_factor =
      compositor_deps->IsUseZoomForDSFEnabled();

  // Build LayerTreeSettings from command line args.
  if (cmd.HasSwitch(cc::switches::kBrowserControlsShowThreshold)) {
    std::string top_threshold_str =
        cmd.GetSwitchValueASCII(cc::switches::kBrowserControlsShowThreshold);
    double show_threshold;
    if (base::StringToDouble(top_threshold_str, &show_threshold) &&
        show_threshold >= 0.f && show_threshold <= 1.f)
      settings.top_controls_show_threshold = show_threshold;
  }

  if (cmd.HasSwitch(cc::switches::kBrowserControlsHideThreshold)) {
    std::string top_threshold_str =
        cmd.GetSwitchValueASCII(cc::switches::kBrowserControlsHideThreshold);
    double hide_threshold;
    if (base::StringToDouble(top_threshold_str, &hide_threshold) &&
        hide_threshold >= 0.f && hide_threshold <= 1.f)
      settings.top_controls_hide_threshold = hide_threshold;
  }

  // Blink sends cc a layer list and property trees when either
  // BlinkGenPropertyTrees or CompositeAfterPaint are enabled.
  settings.use_layer_lists =
      blink::WebRuntimeFeatures::IsBlinkGenPropertyTreesEnabled() ||
      blink::WebRuntimeFeatures::IsCompositeAfterPaintEnabled();

  // Blink currently doesn't support setting fractional scroll offsets so CC
  // must send integer values. We plan to eventually make Blink use fractional
  // offsets internally: https://crbug.com/414283.
  settings.commit_fractional_scroll_deltas =
      blink::WebRuntimeFeatures::IsFractionalScrollOffsetsEnabled();

  // The means the renderer compositor has 2 possible modes:
  // - Threaded compositing with a scheduler.
  // - Single threaded compositing without a scheduler (for web tests only).
  // Using the scheduler in web tests introduces additional composite steps
  // that create flakiness.
  settings.single_thread_proxy_scheduler = false;

  // These flags should be mirrored by UI versions in ui/compositor/.
  if (cmd.HasSwitch(cc::switches::kShowCompositedLayerBorders))
    settings.initial_debug_state.show_debug_borders.set();
  settings.initial_debug_state.show_layer_animation_bounds_rects =
      cmd.HasSwitch(cc::switches::kShowLayerAnimationBounds);
  settings.initial_debug_state.show_paint_rects =
      cmd.HasSwitch(switches::kShowPaintRects);
  settings.initial_debug_state.show_property_changed_rects =
      cmd.HasSwitch(cc::switches::kShowPropertyChangedRects);
  settings.initial_debug_state.show_surface_damage_rects =
      cmd.HasSwitch(cc::switches::kShowSurfaceDamageRects);
  settings.initial_debug_state.show_screen_space_rects =
      cmd.HasSwitch(cc::switches::kShowScreenSpaceRects);

  settings.initial_debug_state.SetRecordRenderingStats(
      cmd.HasSwitch(cc::switches::kEnableGpuBenchmarking));
  settings.enable_surface_synchronization =
      features::IsSurfaceSynchronizationEnabled();
  settings.build_hit_test_data = features::IsVizHitTestingSurfaceLayerEnabled();

  if (cmd.HasSwitch(cc::switches::kSlowDownRasterScaleFactor)) {
    const int kMinSlowDownScaleFactor = 0;
    const int kMaxSlowDownScaleFactor = INT_MAX;
    switch_value_as_int(
        cmd, cc::switches::kSlowDownRasterScaleFactor, kMinSlowDownScaleFactor,
        kMaxSlowDownScaleFactor,
        &settings.initial_debug_state.slow_down_raster_scale_factor);
  }

  // This is default overlay scrollbar settings for Android and DevTools mobile
  // emulator. Aura Overlay Scrollbar will override below.
  settings.scrollbar_animator = cc::LayerTreeSettings::ANDROID_OVERLAY;
  settings.solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(300);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(300);

  if (cmd.HasSwitch(cc::switches::kCCScrollAnimationDurationForTesting)) {
    const int kMinScrollAnimationDuration = 0;
    const int kMaxScrollAnimationDuration = INT_MAX;
    int duration;
    if (switch_value_as_int(cmd,
                            cc::switches::kCCScrollAnimationDurationForTesting,
                            kMinScrollAnimationDuration,
                            kMaxScrollAnimationDuration, &duration)) {
      settings.scroll_animation_duration_for_testing =
          base::TimeDelta::FromSeconds(duration);
    }
  }

#if defined(OS_ANDROID)
  bool using_synchronous_compositor =
      compositor_deps->UsingSynchronousCompositing();
  bool using_low_memory_policy = base::SysInfo::IsLowEndDevice();

  settings.use_stream_video_draw_quad = true;
  settings.using_synchronous_renderer_compositor = using_synchronous_compositor;
  if (using_synchronous_compositor) {
    // Android WebView uses system scrollbars, so make ours invisible.
    // http://crbug.com/677348: This can't be done using hide_scrollbars
    // setting because supporting -webkit custom scrollbars is still desired
    // on sublayers.
    settings.scrollbar_animator = cc::LayerTreeSettings::NO_ANIMATOR;
    settings.solid_color_scrollbar_color = SK_ColorTRANSPARENT;

    settings.enable_early_damage_check =
        cmd.HasSwitch(cc::switches::kCheckDamageEarly);
  }
  // Android WebView handles root layer flings itself.
  settings.ignore_root_layer_flings = using_synchronous_compositor;
  // Memory policy on Android WebView does not depend on whether device is
  // low end, so always use default policy.
  if (using_low_memory_policy && !using_synchronous_compositor) {
    // On low-end we want to be very carefull about killing other
    // apps. So initially we use 50% more memory to avoid flickering
    // or raster-on-demand.
    settings.max_memory_for_prepaint_percentage = 67;
  } else {
    // On other devices we have increased memory excessively to avoid
    // raster-on-demand already, so now we reserve 50% _only_ to avoid
    // raster-on-demand, and use 50% of the memory otherwise.
    settings.max_memory_for_prepaint_percentage = 50;
  }

  // TODO(danakj): Only do this on low end devices.
  settings.create_low_res_tiling = true;

#else   // defined(OS_ANDROID)
  bool using_synchronous_compositor = false;  // Only for Android WebView.
  // On desktop, we never use the low memory policy unless we are simulating
  // low-end mode via a switch.
  bool using_low_memory_policy =
      cmd.HasSwitch(switches::kEnableLowEndDeviceMode);

  if (ui::IsOverlayScrollbarEnabled()) {
    settings.scrollbar_animator = cc::LayerTreeSettings::AURA_OVERLAY;
    settings.scrollbar_fade_delay = ui::kOverlayScrollbarFadeDelay;
    settings.scrollbar_fade_duration = ui::kOverlayScrollbarFadeDuration;
    settings.scrollbar_thinning_duration =
        ui::kOverlayScrollbarThinningDuration;
    settings.scrollbar_flash_after_any_scroll_update =
        ui::OverlayScrollbarFlashAfterAnyScrollUpdate();
    settings.scrollbar_flash_when_mouse_enter =
        ui::OverlayScrollbarFlashWhenMouseEnter();
  }

  // On desktop, if there's over 4GB of memory on the machine, increase the
  // working set size to 256MB for both gpu and software.
  const int kImageDecodeMemoryThresholdMB = 4 * 1024;
  if (base::SysInfo::AmountOfPhysicalMemoryMB() >=
      kImageDecodeMemoryThresholdMB) {
    settings.decoded_image_working_set_budget_bytes = 256 * 1024 * 1024;
  } else {
    // This is the default, but recorded here as well.
    settings.decoded_image_working_set_budget_bytes = 128 * 1024 * 1024;
  }
#endif  // defined(OS_ANDROID)

  if (using_low_memory_policy) {
    // RGBA_4444 textures are only enabled:
    //  - If the user hasn't explicitly disabled them
    //  - If system ram is <= 512MB (1GB devices are sometimes low-end).
    //  - If we are not running in a WebView, where 4444 isn't supported.
    if (!cmd.HasSwitch(switches::kDisableRGBA4444Textures) &&
        base::SysInfo::AmountOfPhysicalMemoryMB() <= 512 &&
        !using_synchronous_compositor) {
      settings.use_rgba_4444 = viz::RGBA_4444;

      // If we are going to unpremultiply and dither these tiles, we need to
      // allocate an additional RGBA_8888 intermediate for each tile
      // rasterization when rastering to RGBA_4444 to allow for dithering.
      // Setting a reasonable sized max tile size allows this intermediate to
      // be consistently reused.
      if (base::FeatureList::IsEnabled(
              kUnpremultiplyAndDitherLowBitDepthTiles)) {
        settings.max_gpu_raster_tile_size = gfx::Size(512, 256);
        settings.unpremultiply_and_dither_low_bit_depth_tiles = true;
      }
    }
  }

  if (cmd.HasSwitch(switches::kEnableLowResTiling))
    settings.create_low_res_tiling = true;
  if (cmd.HasSwitch(switches::kDisableLowResTiling))
    settings.create_low_res_tiling = false;

  if (cmd.HasSwitch(switches::kEnableRGBA4444Textures) &&
      !cmd.HasSwitch(switches::kDisableRGBA4444Textures)) {
    settings.use_rgba_4444 = true;
  }

  settings.max_staging_buffer_usage_in_bytes = 32 * 1024 * 1024;  // 32MB
  // Use 1/4th of staging buffers on low-end devices.
  if (base::SysInfo::IsLowEndDevice())
    settings.max_staging_buffer_usage_in_bytes /= 4;

  cc::ManagedMemoryPolicy defaults = settings.memory_policy;
  settings.memory_policy = GetGpuMemoryPolicy(defaults, initial_screen_size,
                                              initial_device_scale_factor);

  settings.disallow_non_exact_resource_reuse =
      cmd.HasSwitch(switches::kDisallowNonExactResourceReuse);
#if defined(OS_ANDROID)
  // TODO(crbug.com/746931): This feature appears to be causing visual
  // corruption on certain android devices. Will investigate and re-enable.
  settings.disallow_non_exact_resource_reuse = true;
#endif

  if (cmd.HasSwitch(switches::kRunAllCompositorStagesBeforeDraw)) {
    settings.wait_for_all_pipeline_stages_before_draw = true;
    settings.enable_latency_recovery = false;
  }
#if defined(OS_ANDROID)
  if (features::IsSurfaceSynchronizationEnabled()) {
    // TODO(crbug.com/933846): LatencyRecovery is causing jank on Android.
    // Disable in viz mode for now, with plan to disable more widely once
    // viz launches.
    settings.enable_latency_recovery = false;
  }
#endif

  settings.enable_image_animation_resync =
      !cmd.HasSwitch(switches::kDisableImageAnimationResync);

  settings.send_compositor_frame_ack = false;

  return settings;
}

// static
cc::ManagedMemoryPolicy RenderWidget::GetGpuMemoryPolicy(
    const cc::ManagedMemoryPolicy& default_policy,
    const gfx::Size& initial_screen_size,
    float initial_device_scale_factor) {
  cc::ManagedMemoryPolicy actual = default_policy;
  actual.bytes_limit_when_visible = 0;

  // If the value was overridden on the command line, use the specified value.
  static bool client_hard_limit_bytes_overridden =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceGpuMemAvailableMb);
  if (client_hard_limit_bytes_overridden) {
    if (base::StringToSizeT(
            base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                switches::kForceGpuMemAvailableMb),
            &actual.bytes_limit_when_visible))
      actual.bytes_limit_when_visible *= 1024 * 1024;
    return actual;
  }

#if defined(OS_ANDROID)
  // We can't query available GPU memory from the system on Android.
  // Physical memory is also mis-reported sometimes (eg. Nexus 10 reports
  // 1262MB when it actually has 2GB, while Razr M has 1GB but only reports
  // 128MB java heap size). First we estimate physical memory using both.
  size_t dalvik_mb = base::SysInfo::DalvikHeapSizeMB();
  size_t physical_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  size_t physical_memory_mb = 0;
  if (base::SysInfo::IsLowEndDevice()) {
    // TODO(crbug.com/742534): The code below appears to no longer work.
    // |dalvik_mb| no longer follows the expected heuristic pattern, causing us
    // to over-estimate memory on low-end devices. This entire section probably
    // needs to be re-written, but for now we can address the low-end Android
    // issues by ignoring |dalvik_mb|.
    physical_memory_mb = physical_mb;
  } else if (dalvik_mb >= 256) {
    physical_memory_mb = dalvik_mb * 4;
  } else {
    physical_memory_mb = std::max(dalvik_mb * 4, (physical_mb * 4) / 3);
  }

  // Now we take a default of 1/8th of memory on high-memory devices,
  // and gradually scale that back for low-memory devices (to be nicer
  // to other apps so they don't get killed). Examples:
  // Nexus 4/10(2GB)    256MB (normally 128MB)
  // Droid Razr M(1GB)  114MB (normally 57MB)
  // Galaxy Nexus(1GB)  100MB (normally 50MB)
  // Xoom(1GB)          100MB (normally 50MB)
  // Nexus S(low-end)   8MB (normally 8MB)
  // Note that the compositor now uses only some of this memory for
  // pre-painting and uses the rest only for 'emergencies'.
  if (actual.bytes_limit_when_visible == 0) {
    // NOTE: Non-low-end devices use only 50% of these limits,
    // except during 'emergencies' where 100% can be used.
    if (physical_memory_mb >= 1536)
      actual.bytes_limit_when_visible = physical_memory_mb / 8;  // >192MB
    else if (physical_memory_mb >= 1152)
      actual.bytes_limit_when_visible = physical_memory_mb / 8;  // >144MB
    else if (physical_memory_mb >= 768)
      actual.bytes_limit_when_visible = physical_memory_mb / 10;  // >76MB
    else if (physical_memory_mb >= 513)
      actual.bytes_limit_when_visible = physical_memory_mb / 12;  // <64MB
    else
      // Devices with this little RAM have very little headroom so we hardcode
      // the limit rather than relying on the heuristics above.  (They also use
      // 4444 textures so we can use a lower limit.)
      actual.bytes_limit_when_visible = 8;

    actual.bytes_limit_when_visible =
        actual.bytes_limit_when_visible * 1024 * 1024;
    // Clamp the observed value to a specific range on Android.
    actual.bytes_limit_when_visible = std::max(
        actual.bytes_limit_when_visible, static_cast<size_t>(8 * 1024 * 1024));
    actual.bytes_limit_when_visible =
        std::min(actual.bytes_limit_when_visible,
                 static_cast<size_t>(256 * 1024 * 1024));
  }
  actual.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING;
#else
  // Ignore what the system said and give all clients the same maximum
  // allocation on desktop platforms.
  actual.bytes_limit_when_visible = 512 * 1024 * 1024;
  actual.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;

  // For large monitors (4k), double the tile memory to avoid frequent out of
  // memory problems. 4k could mean a screen width of anywhere from 3840 to 4096
  // (see https://en.wikipedia.org/wiki/4K_resolution). We use 3500 as a proxy
  // for "large enough".
  static const int kLargeDisplayThreshold = 3500;
  int display_width =
      std::round(initial_screen_size.width() * initial_device_scale_factor);
  if (display_width >= kLargeDisplayThreshold)
    actual.bytes_limit_when_visible *= 2;
#endif
  return actual;
}

void RenderWidget::HasPointerRawMoveEventHandlers(bool has_handlers) {
  if (input_event_queue_)
    input_event_queue_->HasPointerRawMoveEventHandlers(has_handlers);
}

void RenderWidget::HasTouchEventHandlers(bool has_handlers) {
  if (has_touch_handlers_ && *has_touch_handlers_ == has_handlers)
    return;

  has_touch_handlers_ = has_handlers;
  if (render_widget_scheduling_state_)
    render_widget_scheduling_state_->SetHasTouchHandler(has_handlers);
  Send(new WidgetHostMsg_HasTouchEventHandlers(routing_id_, has_handlers));
}

void RenderWidget::SetNeedsLowLatencyInput(bool needs_low_latency) {
  if (input_event_queue_)
    input_event_queue_->SetNeedsLowLatency(needs_low_latency);
}

void RenderWidget::SetNeedsUnbufferedInputForDebugger(bool unbuffered) {
  if (input_event_queue_)
    input_event_queue_->SetNeedsUnbufferedInputForDebugger(unbuffered);
}

void RenderWidget::AnimateDoubleTapZoomInMainFrame(
    const blink::WebPoint& point,
    const blink::WebRect& rect_to_zoom) {
  // Only oopif subframes should be sending this message.
  DCHECK(!delegate());
  Send(new WidgetHostMsg_AnimateDoubleTapZoomInMainFrame(routing_id(), point,
                                                         rect_to_zoom));
}

void RenderWidget::ZoomToFindInPageRectInMainFrame(
    const blink::WebRect& rect_to_zoom) {
  // Only oopif subframes should be sending this message.
  DCHECK(!delegate_);
  Send(new WidgetHostMsg_ZoomToFindInPageRectInMainFrame(routing_id(),
                                                         rect_to_zoom));
}

void RenderWidget::RegisterViewportLayers(const cc::ViewportLayers& layers) {
  layer_tree_view_->layer_tree_host()->RegisterViewportLayers(layers);
}

void RenderWidget::RegisterSelection(const cc::LayerSelection& selection) {
  layer_tree_view_->layer_tree_host()->RegisterSelection(selection);
}

void RenderWidget::RequestUnbufferedInputEvents() {
  if (input_event_queue_)
    input_event_queue_->RequestUnbufferedInputEvents();
}

void RenderWidget::SetTouchAction(cc::TouchAction touch_action) {
  if (!input_handler_->ProcessTouchAction(touch_action))
    return;

  widget_input_handler_manager_->ProcessTouchAction(touch_action);
}

void RenderWidget::RegisterRenderFrameProxy(RenderFrameProxy* proxy) {
  render_frame_proxies_.AddObserver(proxy);
  // Page scale factor is propagated down the RenderWidget tree (across
  // frame trees). A new RenderFrameProxy means there is a new child
  // RenderWidget in another frame tree. In order for it to hear about
  // the page scale factor we pass along the last seen value here.
  proxy->OnPageScaleFactorChanged(page_scale_factor_from_mainframe_);
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

void RenderWidget::RegisterBrowserPlugin(BrowserPlugin* browser_plugin) {
  browser_plugins_.AddObserver(browser_plugin);
  browser_plugin->ScreenInfoChanged(GetOriginalScreenInfo());
}

void RenderWidget::UnregisterBrowserPlugin(BrowserPlugin* browser_plugin) {
  browser_plugins_.RemoveObserver(browser_plugin);
}

void RenderWidget::OnWaitNextFrameForTests(
    int main_frame_thread_observer_routing_id) {
  // Sends an ACK to the browser process during the next compositor frame.
  QueueMessage(new WidgetHostMsg_WaitForNextFrameForTests_ACK(
      main_frame_thread_observer_routing_id));
}

const ScreenInfo& RenderWidget::GetWebScreenInfo() const {
  return screen_info_;
}

const ScreenInfo& RenderWidget::GetOriginalScreenInfo() const {
  return screen_metrics_emulator_
             ? screen_metrics_emulator_->original_screen_info()
             : screen_info_;
}

gfx::PointF RenderWidget::ConvertWindowPointToViewport(
    const gfx::PointF& point) {
  blink::WebFloatRect point_in_viewport(point.x(), point.y(), 0, 0);
  ConvertWindowToViewport(&point_in_viewport);
  return gfx::PointF(point_in_viewport.x, point_in_viewport.y);
}

gfx::Point RenderWidget::ConvertWindowPointToViewport(const gfx::Point& point) {
  return gfx::ToRoundedPoint(ConvertWindowPointToViewport(gfx::PointF(point)));
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

void RenderWidget::StartDragging(network::mojom::ReferrerPolicy policy,
                                 const WebDragData& data,
                                 WebDragOperationsMask mask,
                                 const SkBitmap& drag_image,
                                 const gfx::Point& web_image_offset) {
  blink::WebRect offset_in_window(web_image_offset.x(), web_image_offset.y(), 0,
                                  0);
  ConvertViewportToWindow(&offset_in_window);
  DropData drop_data(DropDataBuilder::Build(data));
  drop_data.referrer_policy = policy;
  gfx::Vector2d image_offset(offset_in_window.x, offset_in_window.y);
  Send(new DragHostMsg_StartDragging(routing_id(), drop_data, mask, drag_image,
                                     image_offset, possible_drag_event_info_));
}

uint32_t RenderWidget::GetContentSourceId() {
  return current_content_source_id_;
}

void RenderWidget::DidNavigate() {
  // Blink may be navigating still between the Close IPC and the task that
  // actually closes this class, and for a main frame that would come through
  // this method. But since we are closing we can skip it.
  if (closing_)
    return;

  ++current_content_source_id_;
  layer_tree_view_->SetContentSourceId(current_content_source_id_);
  layer_tree_view_->ClearCachesOnNextCommit();
}

blink::WebWidget* RenderWidget::GetWebWidget() const {
  if (delegate()) {
    blink::WebWidget* delegate_widget = delegate()->GetWebWidgetForWidget();
    if (delegate_widget)
      return delegate_widget;
  }
  return webwidget_internal_;
}

blink::WebInputMethodController* RenderWidget::GetInputMethodController()
    const {
  if (auto* frame_widget = GetFrameWidget())
    return frame_widget->GetActiveWebInputMethodController();

  return nullptr;
}

void RenderWidget::SetupWidgetInputHandler(
    mojom::WidgetInputHandlerRequest request,
    mojom::WidgetInputHandlerHostPtr host) {
  widget_input_handler_manager_->AddInterface(std::move(request),
                                              std::move(host));
}

void RenderWidget::SetWidgetBinding(mojom::WidgetRequest request) {
  // Close the old binding if there was one.
  // A RenderWidgetHost should not need more than one channel.
  widget_binding_.Close();
  widget_binding_.Bind(std::move(request));
}

void RenderWidget::SetMouseCapture(bool capture) {
  if (mojom::WidgetInputHandlerHost* host =
          widget_input_handler_manager_->GetWidgetInputHandlerHost()) {
    host->SetMouseCapture(capture);
  }
}

bool RenderWidget::IsSurfaceSynchronizationEnabled() const {
  return layer_tree_view_ &&
         layer_tree_view_->IsSurfaceSynchronizationEnabled();
}

void RenderWidget::PageScaleFactorChanged(float page_scale_factor) {
  // The page scale is controlled by the WebView for the local main frame of
  // the Page. So this is called from blink by for the RenderWidget of that
  // local main frame. We forward the value on to each child RenderWidget (each
  // of which will be via proxy child frame). These will each in turn forward
  // the message to their child RenderWidgets (through their proxy child
  // frames).
  // TODO(crbug.com/924336): This value is continuously propagated during a
  // pinch-zoom, causing the child RenderWidgets to re-raster, while the main
  // frame is able to throttle re-raster to powers of 2. We could find some way
  // to throttle child RenderWidgets also, perhaps by informing them when the
  // pinch-zoom gesture is started and stopped.
  DCHECK(!is_frozen_);
  DCHECK(delegate());

  for (auto& observer : render_frame_proxies_)
    observer.OnPageScaleFactorChanged(page_scale_factor);
  // Store the value to give to any new RenderFrameProxy that is registered.
  page_scale_factor_from_mainframe_ = page_scale_factor;
}

void RenderWidget::UseSynchronousResizeModeForTesting(bool enable) {
  synchronous_resize_mode_for_testing_ = enable;
}

void RenderWidget::SetDeviceScaleFactorForTesting(float factor) {
  DCHECK_GT(factor, 0.f);

  // We are changing the device scale factor from the renderer, so allocate a
  // new viz::LocalSurfaceId to avoid surface invariants violations in tests.
  layer_tree_view_->RequestNewLocalSurfaceId();

  ScreenInfo info = screen_info_;
  info.device_scale_factor = factor;
  gfx::Size viewport_pixel_size = gfx::ScaleToCeiledSize(size_, factor);
  UpdateSurfaceAndScreenInfo(local_surface_id_allocation_from_parent_,
                             viewport_pixel_size, info);

  ResizeWebWidget();  // This picks up the new device scale factor in |info|.

  gfx::Size visible_viewport_size = visible_viewport_size_;
  if (compositor_deps_->IsUseZoomForDSFEnabled()) {
    visible_viewport_size =
        gfx::ScaleToCeiledSize(visible_viewport_size, factor);
  }
  GetWebWidget()->ResizeVisualViewport(visible_viewport_size);

  // Make sure the DSF override stays for future VisualProperties updates, and
  // that includes overriding the VisualProperties'
  // compositor_viewport_pixel_size with size * this for-testing DSF.
  device_scale_factor_for_testing_ = factor;
}

void RenderWidget::SetDeviceColorSpaceForTesting(
    const gfx::ColorSpace& color_space) {
  // We are changing the device color space from the renderer, so allocate a
  // new viz::LocalSurfaceId to avoid surface invariants violations in tests.
  layer_tree_view_->RequestNewLocalSurfaceId();

  ScreenInfo info = screen_info_;
  info.color_space = color_space;
  UpdateSurfaceAndScreenInfo(local_surface_id_allocation_from_parent_,
                             CompositorViewportSize(), info);
}

void RenderWidget::SetWindowRectSynchronouslyForTesting(
    const gfx::Rect& new_window_rect) {
  SetWindowRectSynchronously(new_window_rect);
}

void RenderWidget::EnableAutoResizeForTesting(const gfx::Size& min_size,
                                              const gfx::Size& max_size) {
  VisualProperties visual_properties;
  visual_properties.auto_resize_enabled = true;
  visual_properties.min_size_for_auto_resize = min_size;
  visual_properties.max_size_for_auto_resize = max_size;
  visual_properties.local_surface_id_allocation =
      base::Optional<viz::LocalSurfaceIdAllocation>(
          viz::LocalSurfaceIdAllocation(
              viz::LocalSurfaceId(1, 1, base::UnguessableToken::Create()),
              base::TimeTicks::Now()));
  OnSynchronizeVisualProperties(visual_properties);
}

void RenderWidget::DisableAutoResizeForTesting(const gfx::Size& new_size) {
  if (!auto_resize_mode_)
    return;

  VisualProperties visual_properties;
  visual_properties.auto_resize_enabled = false;
  visual_properties.screen_info = screen_info_;
  visual_properties.new_size = new_size;
  visual_properties.compositor_viewport_pixel_size = CompositorViewportSize();
  visual_properties.browser_controls_shrink_blink_size =
      browser_controls_shrink_blink_size_;
  visual_properties.top_controls_height = top_controls_height_;
  visual_properties.visible_viewport_size = visible_viewport_size_;
  visual_properties.is_fullscreen_granted = is_fullscreen_granted_;
  visual_properties.display_mode = display_mode_;
  OnSynchronizeVisualProperties(visual_properties);
}

blink::WebLocalFrame* RenderWidget::GetFocusedWebLocalFrameInWidget() const {
  if (auto* frame_widget = GetFrameWidget())
    return frame_widget->FocusedWebLocalFrameInWidget();
  return nullptr;
}

#if BUILDFLAG(ENABLE_PLUGINS)
PepperPluginInstanceImpl* RenderWidget::GetFocusedPepperPluginInsideWidget() {
  blink::WebFrameWidget* frame_widget = GetFrameWidget();
  if (!frame_widget)
    return nullptr;

  // Focused pepper instance might not always be in the focused frame. For
  // instance if a pepper instance and its embedder frame are focused an then
  // another frame takes focus using javascript, the embedder frame will no
  // longer be focused while the pepper instance is (the embedder frame's
  // |focused_pepper_plugin_| is not nullptr). Especially, if the pepper plugin
  // is fullscreen, clicking into the pepper will not refocus the embedder
  // frame. This is why we have to traverse the whole frame tree to find the
  // focused plugin.
  blink::WebFrame* current_frame = frame_widget->LocalRoot();
  while (current_frame) {
    RenderFrameImpl* render_frame =
        current_frame->IsWebLocalFrame()
            ? RenderFrameImpl::FromWebFrame(current_frame)
            : nullptr;
    if (render_frame && render_frame->focused_pepper_plugin())
      return render_frame->focused_pepper_plugin();
    current_frame = current_frame->TraverseNext();
  }
  return nullptr;
}
#endif

gfx::Rect RenderWidget::ViewportVisibleRect() {
  if (for_child_local_root_frame_)
    return compositor_visible_rect_;
  return gfx::Rect(CompositorViewportSize());
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
RenderWidget::GetCleanupTaskRunner() {
  return RenderThreadImpl::current_blink_platform_impl()
      ->main_thread_scheduler()
      ->CleanupTaskRunner();
}

base::WeakPtr<RenderWidget> RenderWidget::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace content
