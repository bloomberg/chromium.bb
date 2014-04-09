// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/auto_reset.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "cc/layers/delegated_frame_provider.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/trees/layer_tree_settings.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/backing_store_aura.h"
#include "content/browser/renderer_host/compositor_resize_lock_aura.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_aura.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "content/browser/renderer_host/web_input_event_aura.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_widget_host_view_frame_subscriber.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/scoped_tooltip_disabler.h"
#include "ui/wm/public/tooltip_client.h"
#include "ui/wm/public/transient_window_client.h"
#include "ui/wm/public/window_types.h"

#if defined(OS_WIN)
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/common/plugin_constants_win.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/win/dpi.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "content/common/input_messages.h"
#include "ui/events/linux/text_edit_command_auralinux.h"
#include "ui/events/linux/text_edit_key_bindings_delegate_auralinux.h"
#endif

using gfx::RectToSkIRect;
using gfx::SkIRectToRect;

using blink::WebScreenInfo;
using blink::WebTouchEvent;

namespace content {

namespace {

// In mouse lock mode, we need to prevent the (invisible) cursor from hitting
// the border of the view, in order to get valid movement information. However,
// forcing the cursor back to the center of the view after each mouse move
// doesn't work well. It reduces the frequency of useful mouse move messages
// significantly. Therefore, we move the cursor to the center of the view only
// if it approaches the border. |kMouseLockBorderPercentage| specifies the width
// of the border area, in percentage of the corresponding dimension.
const int kMouseLockBorderPercentage = 15;

// When accelerated compositing is enabled and a widget resize is pending,
// we delay further resizes of the UI. The following constant is the maximum
// length of time that we should delay further UI resizes while waiting for a
// resized frame from a renderer.
const int kResizeLockTimeoutMs = 67;

#if defined(OS_WIN)
// Used to associate a plugin HWND with its RenderWidgetHostViewAura instance.
const wchar_t kWidgetOwnerProperty[] = L"RenderWidgetHostViewAuraOwner";

BOOL CALLBACK WindowDestroyingCallback(HWND window, LPARAM param) {
  RenderWidgetHostViewAura* widget =
      reinterpret_cast<RenderWidgetHostViewAura*>(param);
  if (GetProp(window, kWidgetOwnerProperty) == widget) {
    // Properties set on HWNDs must be removed to avoid leaks.
    RemoveProp(window, kWidgetOwnerProperty);
    RenderWidgetHostViewBase::DetachPluginWindowsCallback(window);
  }
  return TRUE;
}

BOOL CALLBACK HideWindowsCallback(HWND window, LPARAM param) {
  RenderWidgetHostViewAura* widget =
      reinterpret_cast<RenderWidgetHostViewAura*>(param);
  if (GetProp(window, kWidgetOwnerProperty) == widget)
    SetParent(window, ui::GetHiddenWindow());
  return TRUE;
}

BOOL CALLBACK ShowWindowsCallback(HWND window, LPARAM param) {
  RenderWidgetHostViewAura* widget =
      reinterpret_cast<RenderWidgetHostViewAura*>(param);

  if (GetProp(window, kWidgetOwnerProperty) == widget &&
      widget->GetNativeView()->GetHost()) {
    HWND parent = widget->GetNativeView()->GetHost()->GetAcceleratedWidget();
    SetParent(window, parent);
  }
  return TRUE;
}

struct CutoutRectsParams {
  RenderWidgetHostViewAura* widget;
  std::vector<gfx::Rect> cutout_rects;
  std::map<HWND, WebPluginGeometry>* geometry;
};

// Used to update the region for the windowed plugin to draw in. We start with
// the clip rect from the renderer, then remove the cutout rects from the
// renderer, and then remove the transient windows from the root window and the
// constrained windows from the parent window.
BOOL CALLBACK SetCutoutRectsCallback(HWND window, LPARAM param) {
  CutoutRectsParams* params = reinterpret_cast<CutoutRectsParams*>(param);

  if (GetProp(window, kWidgetOwnerProperty) == params->widget) {
    // First calculate the offset of this plugin from the root window, since
    // the cutouts are relative to the root window.
    HWND parent =
        params->widget->GetNativeView()->GetHost()->GetAcceleratedWidget();
    POINT offset;
    offset.x = offset.y = 0;
    MapWindowPoints(window, parent, &offset, 1);

    // Now get the cached clip rect and cutouts for this plugin window that came
    // from the renderer.
    std::map<HWND, WebPluginGeometry>::iterator i = params->geometry->begin();
    while (i != params->geometry->end() &&
           i->second.window != window &&
           GetParent(i->second.window) != window) {
      ++i;
    }

    if (i == params->geometry->end()) {
      NOTREACHED();
      return TRUE;
    }

    HRGN hrgn = CreateRectRgn(i->second.clip_rect.x(),
                              i->second.clip_rect.y(),
                              i->second.clip_rect.right(),
                              i->second.clip_rect.bottom());
    // We start with the cutout rects that came from the renderer, then add the
    // ones that came from transient and constrained windows.
    std::vector<gfx::Rect> cutout_rects = i->second.cutout_rects;
    for (size_t i = 0; i < params->cutout_rects.size(); ++i) {
      gfx::Rect offset_cutout = params->cutout_rects[i];
      offset_cutout.Offset(-offset.x, -offset.y);
      cutout_rects.push_back(offset_cutout);
    }
    gfx::SubtractRectanglesFromRegion(hrgn, cutout_rects);
    // If we don't have any cutout rects then no point in messing with the
    // window region.
    if (cutout_rects.size())
      SetWindowRgn(window, hrgn, TRUE);
  }
  return TRUE;
}

// A callback function for EnumThreadWindows to enumerate and dismiss
// any owned popup windows.
BOOL CALLBACK DismissOwnedPopups(HWND window, LPARAM arg) {
  const HWND toplevel_hwnd = reinterpret_cast<HWND>(arg);

  if (::IsWindowVisible(window)) {
    const HWND owner = ::GetWindow(window, GW_OWNER);
    if (toplevel_hwnd == owner) {
      ::PostMessage(window, WM_CANCELMODE, 0, 0);
    }
  }

  return TRUE;
}
#endif

void UpdateWebTouchEventAfterDispatch(blink::WebTouchEvent* event,
                                      blink::WebTouchPoint* point) {
  if (point->state != blink::WebTouchPoint::StateReleased &&
      point->state != blink::WebTouchPoint::StateCancelled)
    return;
  --event->touchesLength;
  for (unsigned i = point - event->touches;
       i < event->touchesLength;
       ++i) {
    event->touches[i] = event->touches[i + 1];
  }
}

bool CanRendererHandleEvent(const ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED)
    return false;

#if defined(OS_WIN)
  // Renderer cannot handle WM_XBUTTON or NC events.
  switch (event->native_event().message) {
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_NCMOUSELEAVE:
    case WM_NCMOUSEMOVE:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
    case WM_NCRBUTTONDBLCLK:
    case WM_NCMBUTTONDOWN:
    case WM_NCMBUTTONUP:
    case WM_NCMBUTTONDBLCLK:
    case WM_NCXBUTTONDOWN:
    case WM_NCXBUTTONUP:
    case WM_NCXBUTTONDBLCLK:
      return false;
    default:
      break;
  }
#elif defined(USE_X11)
  // Renderer only supports standard mouse buttons, so ignore programmable
  // buttons.
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_RELEASED:
      return event->IsAnyButton();
    default:
      break;
  }
#endif
  return true;
}

// We don't mark these as handled so that they're sent back to the
// DefWindowProc so it can generate WM_APPCOMMAND as necessary.
bool IsXButtonUpEvent(const ui::MouseEvent* event) {
#if defined(OS_WIN)
  switch (event->native_event().message) {
    case WM_XBUTTONUP:
    case WM_NCXBUTTONUP:
      return true;
  }
#endif
  return false;
}

void GetScreenInfoForWindow(WebScreenInfo* results, aura::Window* window) {
  const gfx::Display display = window ?
      gfx::Screen::GetScreenFor(window)->GetDisplayNearestWindow(window) :
      gfx::Screen::GetScreenFor(window)->GetPrimaryDisplay();
  results->rect = display.bounds();
  results->availableRect = display.work_area();
  // TODO(derat|oshima): Don't hardcode this. Get this from display object.
  results->depth = 24;
  results->depthPerComponent = 8;
  results->deviceScaleFactor = display.device_scale_factor();
}

bool PointerEventActivates(const ui::Event& event) {
  if (event.type() == ui::ET_MOUSE_PRESSED)
    return true;

  if (event.type() == ui::ET_GESTURE_BEGIN) {
    const ui::GestureEvent& gesture =
        static_cast<const ui::GestureEvent&>(event);
    return gesture.details().touch_points() == 1;
  }

  return false;
}

}  // namespace

// We need to watch for mouse events outside a Web Popup or its parent
// and dismiss the popup for certain events.
class RenderWidgetHostViewAura::EventFilterForPopupExit
    : public ui::EventHandler {
 public:
  explicit EventFilterForPopupExit(RenderWidgetHostViewAura* rwhva)
      : rwhva_(rwhva) {
    DCHECK(rwhva_);
    aura::Env::GetInstance()->AddPreTargetHandler(this);
  }

  virtual ~EventFilterForPopupExit() {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
  }

  // Overridden from ui::EventHandler
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    rwhva_->ApplyEventFilterForPopupExit(event);
  }

  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE {
    rwhva_->ApplyEventFilterForPopupExit(event);
  }

 private:
  RenderWidgetHostViewAura* rwhva_;

  DISALLOW_COPY_AND_ASSIGN(EventFilterForPopupExit);
};

void RenderWidgetHostViewAura::ApplyEventFilterForPopupExit(
    ui::LocatedEvent* event) {
  if (in_shutdown_ || is_fullscreen_ || !event->target())
    return;

  if (event->type() != ui::ET_MOUSE_PRESSED &&
      event->type() != ui::ET_TOUCH_PRESSED) {
    return;
  }

  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (target != window_ &&
      (!popup_parent_host_view_ ||
       target != popup_parent_host_view_->window_)) {
    // Note: popup_parent_host_view_ may be NULL when there are multiple
    // popup children per view. See: RenderWidgetHostViewAura::InitAsPopup().
    in_shutdown_ = true;
    host_->Shutdown();
  }
}

// We have to implement the WindowObserver interface on a separate object
// because clang doesn't like implementing multiple interfaces that have
// methods with the same name. This object is owned by the
// RenderWidgetHostViewAura.
class RenderWidgetHostViewAura::WindowObserver : public aura::WindowObserver {
 public:
  explicit WindowObserver(RenderWidgetHostViewAura* view)
      : view_(view) {
    view_->window_->AddObserver(this);
  }

  virtual ~WindowObserver() {
    view_->window_->RemoveObserver(this);
  }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowAddedToRootWindow(aura::Window* window) OVERRIDE {
    if (window == view_->window_)
      view_->AddedToRootWindow();
  }

  virtual void OnWindowRemovingFromRootWindow(aura::Window* window,
                                              aura::Window* new_root) OVERRIDE {
    if (window == view_->window_)
      view_->RemovingFromRootWindow();
  }

 private:
  RenderWidgetHostViewAura* view_;

  DISALLOW_COPY_AND_ASSIGN(WindowObserver);
};

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, public:

RenderWidgetHostViewAura::RenderWidgetHostViewAura(RenderWidgetHost* host)
    : host_(RenderWidgetHostImpl::From(host)),
      window_(new aura::Window(this)),
      in_shutdown_(false),
      in_bounds_changed_(false),
      is_fullscreen_(false),
      popup_parent_host_view_(NULL),
      popup_child_host_view_(NULL),
      is_loading_(false),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      text_input_mode_(ui::TEXT_INPUT_MODE_DEFAULT),
      can_compose_inline_(true),
      has_composition_text_(false),
      accept_return_character_(false),
      last_output_surface_id_(0),
      pending_delegated_ack_count_(0),
      skipped_frames_(false),
      last_swapped_software_frame_scale_factor_(1.f),
      paint_canvas_(NULL),
      synthetic_move_sent_(false),
      can_lock_compositor_(YES),
      cursor_visibility_state_in_renderer_(UNKNOWN),
      touch_editing_client_(NULL),
      delegated_frame_evictor_(new DelegatedFrameEvictor(this)),
      weak_ptr_factory_(this) {
  host_->SetView(this);
  window_observer_.reset(new WindowObserver(this));
  aura::client::SetTooltipText(window_, &tooltip_);
  aura::client::SetActivationDelegate(window_, this);
  aura::client::SetActivationChangeObserver(window_, this);
  aura::client::SetFocusChangeObserver(window_, this);
  window_->set_layer_owner_delegate(this);
  gfx::Screen::GetScreenFor(window_)->AddObserver(this);
  ImageTransportFactory::GetInstance()->AddObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, RenderWidgetHostView implementation:

bool RenderWidgetHostViewAura::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetHostViewAura, message)
    // TODO(kevers): Move to RenderWidgetHostViewImpl and consolidate IPC
    // messages for TextInput<State|Type>Changed. Corresponding code in
    // RenderWidgetHostViewAndroid should also be moved at the same time.
    IPC_MESSAGE_HANDLER(ViewHostMsg_TextInputStateChanged,
                        OnTextInputStateChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderWidgetHostViewAura::InitAsChild(
    gfx::NativeView parent_view) {
  window_->SetType(ui::wm::WINDOW_TYPE_CONTROL);
  window_->Init(aura::WINDOW_LAYER_TEXTURED);
  window_->SetName("RenderWidgetHostViewAura");
}

void RenderWidgetHostViewAura::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds_in_screen) {
  popup_parent_host_view_ =
      static_cast<RenderWidgetHostViewAura*>(parent_host_view);

  // TransientWindowClient may be NULL during tests.
  aura::client::TransientWindowClient* transient_window_client =
      aura::client::GetTransientWindowClient();
  RenderWidgetHostViewAura* old_child =
      popup_parent_host_view_->popup_child_host_view_;
  if (old_child) {
    // TODO(jhorwich): Allow multiple popup_child_host_view_ per view, or
    // similar mechanism to ensure a second popup doesn't cause the first one
    // to never get a chance to filter events. See crbug.com/160589.
    DCHECK(old_child->popup_parent_host_view_ == popup_parent_host_view_);
    if (transient_window_client) {
      transient_window_client->RemoveTransientChild(
        popup_parent_host_view_->window_, old_child->window_);
    }
    old_child->popup_parent_host_view_ = NULL;
  }
  popup_parent_host_view_->popup_child_host_view_ = this;
  window_->SetType(ui::wm::WINDOW_TYPE_MENU);
  window_->Init(aura::WINDOW_LAYER_TEXTURED);
  window_->SetName("RenderWidgetHostViewAura");

  aura::Window* root = popup_parent_host_view_->window_->GetRootWindow();
  aura::client::ParentWindowWithContext(window_, root, bounds_in_screen);
  // Setting the transient child allows for the popup to get mouse events when
  // in a system modal dialog.
  // This fixes crbug.com/328593.
  if (transient_window_client) {
    transient_window_client->AddTransientChild(
        popup_parent_host_view_->window_, window_);
  }

  SetBounds(bounds_in_screen);
  Show();
#if !defined(OS_WIN) && !defined(OS_CHROMEOS)
  if (NeedsInputGrab())
    window_->SetCapture();
#endif

  event_filter_for_popup_exit_.reset(new EventFilterForPopupExit(this));
}

void RenderWidgetHostViewAura::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  is_fullscreen_ = true;
  window_->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window_->Init(aura::WINDOW_LAYER_TEXTURED);
  window_->SetName("RenderWidgetHostViewAura");
  window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);

  aura::Window* parent = NULL;
  gfx::Rect bounds;
  if (reference_host_view) {
    aura::Window* reference_window =
        static_cast<RenderWidgetHostViewAura*>(reference_host_view)->window_;
    if (reference_window) {
      host_tracker_.reset(new aura::WindowTracker);
      host_tracker_->Add(reference_window);
    }
    gfx::Display display = gfx::Screen::GetScreenFor(window_)->
        GetDisplayNearestWindow(reference_window);
    parent = reference_window->GetRootWindow();
    bounds = display.bounds();
  }
  aura::client::ParentWindowWithContext(window_, parent, bounds);
  Show();
  Focus();
}

RenderWidgetHost* RenderWidgetHostViewAura::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewAura::WasShown() {
  DCHECK(host_);
  if (!host_->is_hidden())
    return;
  host_->WasShown();
  delegated_frame_evictor_->SetVisible(true);

  aura::Window* root = window_->GetRootWindow();
  if (root) {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(root);
    if (cursor_client)
      NotifyRendererOfCursorVisibilityState(cursor_client->IsCursorVisible());
  }

  if (host_->is_accelerated_compositing_active() &&
      !released_front_lock_.get()) {
    ui::Compositor* compositor = GetCompositor();
    if (compositor)
      released_front_lock_ = compositor->GetCompositorLock();
  }

#if defined(OS_WIN)
  if (legacy_render_widget_host_HWND_) {
    // Reparent the legacy Chrome_RenderWidgetHostHWND window to the parent
    // window before reparenting any plugins. This ensures that the plugin
    // windows stay on top of the child Zorder in the parent and receive
    // mouse events, etc.
    legacy_render_widget_host_HWND_->UpdateParent(
        GetNativeView()->GetHost()->GetAcceleratedWidget());
    legacy_render_widget_host_HWND_->SetBounds(
        window_->GetBoundsInRootWindow());
  }
  LPARAM lparam = reinterpret_cast<LPARAM>(this);
  EnumChildWindows(ui::GetHiddenWindow(), ShowWindowsCallback, lparam);
#endif
}

void RenderWidgetHostViewAura::WasHidden() {
  if (!host_ || host_->is_hidden())
    return;
  host_->WasHidden();
  delegated_frame_evictor_->SetVisible(false);
  released_front_lock_ = NULL;

#if defined(OS_WIN)
  constrained_rects_.clear();
  aura::WindowTreeHost* host = window_->GetHost();
  if (host) {
    HWND parent = host->GetAcceleratedWidget();
    LPARAM lparam = reinterpret_cast<LPARAM>(this);
    EnumChildWindows(parent, HideWindowsCallback, lparam);
    // We reparent the legacy Chrome_RenderWidgetHostHWND window to the global
    // hidden window on the same lines as Windowed plugin windows.
    if (legacy_render_widget_host_HWND_)
      legacy_render_widget_host_HWND_->UpdateParent(ui::GetHiddenWindow());
  }
#endif
}

void RenderWidgetHostViewAura::SetSize(const gfx::Size& size) {
  // For a SetSize operation, we don't care what coordinate system the origin
  // of the window is in, it's only important to make sure that the origin
  // remains constant after the operation.
  InternalSetBounds(gfx::Rect(window_->bounds().origin(), size));
}

void RenderWidgetHostViewAura::SetBounds(const gfx::Rect& rect) {
  gfx::Point relative_origin(rect.origin());

  // RenderWidgetHostViewAura::SetBounds() takes screen coordinates, but
  // Window::SetBounds() takes parent coordinates, so do the conversion here.
  aura::Window* root = window_->GetRootWindow();
  if (root) {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root);
    if (screen_position_client) {
      screen_position_client->ConvertPointFromScreen(
          window_->parent(), &relative_origin);
    }
  }

  InternalSetBounds(gfx::Rect(relative_origin, rect.size()));
}

void RenderWidgetHostViewAura::MaybeCreateResizeLock() {
  if (!ShouldCreateResizeLock())
    return;
  DCHECK(window_->GetHost());
  DCHECK(window_->GetHost()->compositor());

  // Listen to changes in the compositor lock state.
  ui::Compositor* compositor = window_->GetHost()->compositor();
  if (!compositor->HasObserver(this))
    compositor->AddObserver(this);

  bool defer_compositor_lock =
      can_lock_compositor_ == NO_PENDING_RENDERER_FRAME ||
      can_lock_compositor_ == NO_PENDING_COMMIT;

  if (can_lock_compositor_ == YES)
    can_lock_compositor_ = YES_DID_LOCK;

  resize_lock_ = CreateResizeLock(defer_compositor_lock);
}

bool RenderWidgetHostViewAura::ShouldCreateResizeLock() {
  // On Windows while resizing, the the resize locks makes us mis-paint a white
  // vertical strip (including the non-client area) if the content composition
  // is lagging the UI composition. So here we disable the throttling so that
  // the UI bits can draw ahead of the content thereby reducing the amount of
  // whiteout. Because this causes the content to be drawn at wrong sizes while
  // resizing we compensate by blocking the UI thread in Compositor::Draw() by
  // issuing a FinishAllRendering() if we are resizing.
#if defined(OS_WIN)
  return false;
#else
  if (resize_lock_)
    return false;

  if (host_->should_auto_resize())
    return false;
  if (!host_->is_accelerated_compositing_active())
    return false;

  gfx::Size desired_size = window_->bounds().size();
  if (desired_size == current_frame_size_in_dip_)
    return false;

  aura::WindowTreeHost* host = window_->GetHost();
  if (!host)
    return false;

  ui::Compositor* compositor = host->compositor();
  if (!compositor)
    return false;

  return true;
#endif
}

scoped_ptr<ResizeLock> RenderWidgetHostViewAura::CreateResizeLock(
    bool defer_compositor_lock) {
  gfx::Size desired_size = window_->bounds().size();
  return scoped_ptr<ResizeLock>(new CompositorResizeLock(
      window_->GetHost(),
      desired_size,
      defer_compositor_lock,
      base::TimeDelta::FromMilliseconds(kResizeLockTimeoutMs)));
}

void RenderWidgetHostViewAura::RequestCopyOfOutput(
    scoped_ptr<cc::CopyOutputRequest> request) {
  window_->layer()->RequestCopyOfOutput(request.Pass());
}

gfx::NativeView RenderWidgetHostViewAura::GetNativeView() const {
  return window_;
}

gfx::NativeViewId RenderWidgetHostViewAura::GetNativeViewId() const {
#if defined(OS_WIN)
  aura::WindowTreeHost* host = window_->GetHost();
  if (host)
    return reinterpret_cast<gfx::NativeViewId>(host->GetAcceleratedWidget());
#endif
  return static_cast<gfx::NativeViewId>(NULL);
}

gfx::NativeViewAccessible RenderWidgetHostViewAura::GetNativeViewAccessible() {
#if defined(OS_WIN)
  aura::WindowTreeHost* host = window_->GetHost();
  if (!host)
    return static_cast<gfx::NativeViewAccessible>(NULL);
  HWND hwnd = host->GetAcceleratedWidget();

  CreateBrowserAccessibilityManagerIfNeeded();
  BrowserAccessibilityManager* manager = GetBrowserAccessibilityManager();
  if (manager)
    return manager->GetRoot()->ToBrowserAccessibilityWin();
#endif

  NOTIMPLEMENTED();
  return static_cast<gfx::NativeViewAccessible>(NULL);
}

void RenderWidgetHostViewAura::SetKeyboardFocus() {
#if defined(OS_WIN)
  if (CanFocus()) {
    aura::WindowTreeHost* host = window_->GetHost();
    if (host)
      ::SetFocus(host->GetAcceleratedWidget());
  }
#endif
}

RenderFrameHostImpl* RenderWidgetHostViewAura::GetFocusedFrame() {
  if (!host_->IsRenderView())
    return NULL;
  RenderViewHost* rvh = RenderViewHost::From(host_);
  FrameTreeNode* focused_frame =
      rvh->GetDelegate()->GetFrameTree()->GetFocusedFrame();
  if (!focused_frame)
    return NULL;

  return focused_frame->current_frame_host();
}

void RenderWidgetHostViewAura::MovePluginWindows(
    const gfx::Vector2d& scroll_offset,
    const std::vector<WebPluginGeometry>& plugin_window_moves) {
#if defined(OS_WIN)
  // We need to clip the rectangle to the tab's viewport, otherwise we will draw
  // over the browser UI.
  if (!window_->GetRootWindow()) {
    DCHECK(plugin_window_moves.empty());
    return;
  }
  HWND parent = window_->GetHost()->GetAcceleratedWidget();
  gfx::Rect view_bounds = window_->GetBoundsInRootWindow();
  std::vector<WebPluginGeometry> moves = plugin_window_moves;

  gfx::Rect view_port(scroll_offset.x(), scroll_offset.y(), view_bounds.width(),
                      view_bounds.height());

  for (size_t i = 0; i < moves.size(); ++i) {
    gfx::Rect clip(moves[i].clip_rect);
    gfx::Vector2d view_port_offset(
        moves[i].window_rect.OffsetFromOrigin() + scroll_offset);
    clip.Offset(view_port_offset);
    clip.Intersect(view_port);
    clip.Offset(-view_port_offset);
    moves[i].clip_rect = clip;

    moves[i].window_rect.Offset(view_bounds.OffsetFromOrigin());

    plugin_window_moves_[moves[i].window] = moves[i];

    // constrained_rects_ are relative to the root window. We want to convert
    // them to be relative to the plugin window.
    for (size_t j = 0; j < constrained_rects_.size(); ++j) {
      gfx::Rect offset_cutout = constrained_rects_[j];
      offset_cutout -= moves[i].window_rect.OffsetFromOrigin();
      moves[i].cutout_rects.push_back(offset_cutout);
    }
  }

  MovePluginWindowsHelper(parent, moves);

  // Make sure each plugin window (or its wrapper if it exists) has a pointer to
  // |this|.
  for (size_t i = 0; i < moves.size(); ++i) {
    HWND window = moves[i].window;
    if (GetParent(window) != parent) {
      window = GetParent(window);
    }
    if (!GetProp(window, kWidgetOwnerProperty))
      SetProp(window, kWidgetOwnerProperty, this);
  }
#endif  // defined(OS_WIN)
}

void RenderWidgetHostViewAura::Focus() {
  // Make sure we have a FocusClient before attempting to Focus(). In some
  // situations we may not yet be in a valid Window hierarchy (such as reloading
  // after out of memory discarded the tab).
  aura::client::FocusClient* client = aura::client::GetFocusClient(window_);
  if (client)
    window_->Focus();
}

void RenderWidgetHostViewAura::Blur() {
  window_->Blur();
}

bool RenderWidgetHostViewAura::HasFocus() const {
  return window_->HasFocus();
}

bool RenderWidgetHostViewAura::IsSurfaceAvailableForCopy() const {
  return CanCopyToBitmap() || !!host_->GetBackingStore(false);
}

void RenderWidgetHostViewAura::Show() {
  window_->Show();
  WasShown();
#if defined(OS_WIN)
  if (legacy_render_widget_host_HWND_)
    legacy_render_widget_host_HWND_->Show();
#endif
}

void RenderWidgetHostViewAura::Hide() {
  window_->Hide();
  WasHidden();
#if defined(OS_WIN)
  if (legacy_render_widget_host_HWND_)
    legacy_render_widget_host_HWND_->Hide();
#endif
}

bool RenderWidgetHostViewAura::IsShowing() {
  return window_->IsVisible();
}

gfx::Rect RenderWidgetHostViewAura::GetViewBounds() const {
  // This is the size that we want the renderer to produce. While we're waiting
  // for the correct frame (i.e. during a resize), don't change the size so that
  // we don't pipeline more resizes than we can handle.
  gfx::Rect bounds(window_->GetBoundsInScreen());
  if (resize_lock_.get())
    return gfx::Rect(bounds.origin(), resize_lock_->expected_size());
  else
    return bounds;
}

void RenderWidgetHostViewAura::SetBackground(const SkBitmap& background) {
  RenderWidgetHostViewBase::SetBackground(background);
  host_->SetBackground(background);
  window_->layer()->SetFillsBoundsOpaquely(background.isOpaque());
}

void RenderWidgetHostViewAura::UpdateCursor(const WebCursor& cursor) {
  current_cursor_ = cursor;
  const gfx::Display display = gfx::Screen::GetScreenFor(window_)->
      GetDisplayNearestWindow(window_);
  current_cursor_.SetDisplayInfo(display);
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::TextInputTypeChanged(
    ui::TextInputType type,
    ui::TextInputMode input_mode,
    bool can_compose_inline) {
  if (text_input_type_ != type ||
      text_input_mode_ != input_mode ||
      can_compose_inline_ != can_compose_inline) {
    text_input_type_ = type;
    text_input_mode_ = input_mode;
    can_compose_inline_ = can_compose_inline;
    if (GetInputMethod())
      GetInputMethod()->OnTextInputTypeChanged(this);
    if (touch_editing_client_)
      touch_editing_client_->OnTextInputTypeChanged(text_input_type_);
  }
}

void RenderWidgetHostViewAura::OnTextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  if (params.show_ime_if_needed && params.type != ui::TEXT_INPUT_TYPE_NONE) {
    if (GetInputMethod())
      GetInputMethod()->ShowImeIfNeeded();
  }
}

void RenderWidgetHostViewAura::ImeCancelComposition() {
  if (GetInputMethod())
    GetInputMethod()->CancelComposition(this);
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  composition_character_bounds_ = character_bounds;
}

void RenderWidgetHostViewAura::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect,
    const gfx::Vector2d& scroll_delta,
    const std::vector<gfx::Rect>& copy_rects,
    const std::vector<ui::LatencyInfo>& latency_info) {
  for (size_t i = 0; i < latency_info.size(); i++)
    software_latency_info_.push_back(latency_info[i]);

  // Use the state of the RenderWidgetHost and not the window as the two may
  // differ. In particular if the window is hidden but the renderer isn't and we
  // ignore the update and the window is made visible again the layer isn't
  // marked as dirty and we show the wrong thing.
  if (host_->is_hidden())
    return;

  gfx::Rect clip_rect;
  if (paint_canvas_) {
    SkRect sk_clip_rect;
    if (paint_canvas_->sk_canvas()->getClipBounds(&sk_clip_rect))
      clip_rect = gfx::ToEnclosingRect(gfx::SkRectToRectF(sk_clip_rect));
  }

  if (!scroll_rect.IsEmpty())
    SchedulePaintIfNotInClip(scroll_rect, clip_rect);

#if defined(OS_WIN)
  aura::WindowTreeHost* host = window_->GetHost();
#endif
  for (size_t i = 0; i < copy_rects.size(); ++i) {
    gfx::Rect rect = gfx::SubtractRects(copy_rects[i], scroll_rect);
    if (rect.IsEmpty())
      continue;

    SchedulePaintIfNotInClip(rect, clip_rect);

#if defined(OS_WIN)
    if (host) {
      // Send the invalid rect in screen coordinates.
      gfx::Rect screen_rect = GetViewBounds();
      gfx::Rect invalid_screen_rect(rect);
      invalid_screen_rect.Offset(screen_rect.x(), screen_rect.y());
      HWND hwnd = host->GetAcceleratedWidget();
      PaintPluginWindowsHelper(hwnd, invalid_screen_rect);
    }
#endif  // defined(OS_WIN)
  }
}

void RenderWidgetHostViewAura::RenderProcessGone(base::TerminationStatus status,
                                                 int error_code) {
  UpdateCursorIfOverSelf();
  Destroy();
}

void RenderWidgetHostViewAura::Destroy() {
  // Beware, this function is not called on all destruction paths. It will
  // implicitly end up calling ~RenderWidgetHostViewAura though, so all
  // destruction/cleanup code should happen there, not here.
  in_shutdown_ = true;
  delete window_;
}

void RenderWidgetHostViewAura::SetTooltipText(
    const base::string16& tooltip_text) {
  tooltip_ = tooltip_text;
  aura::Window* root_window = window_->GetRootWindow();
  aura::client::TooltipClient* tooltip_client =
      aura::client::GetTooltipClient(root_window);
  if (tooltip_client) {
    tooltip_client->UpdateTooltip(window_);
    // Content tooltips should be visible indefinitely.
    tooltip_client->SetTooltipShownTimeout(window_, 0);
  }
}

void RenderWidgetHostViewAura::SelectionChanged(const base::string16& text,
                                                size_t offset,
                                                const gfx::Range& range) {
  RenderWidgetHostViewBase::SelectionChanged(text, offset, range);

#if defined(USE_X11) && !defined(OS_CHROMEOS)
  if (text.empty() || range.is_empty())
    return;
  size_t pos = range.GetMin() - offset;
  size_t n = range.length();

  DCHECK(pos + n <= text.length()) << "The text can not fully cover range.";
  if (pos >= text.length()) {
    NOTREACHED() << "The text can not cover range.";
    return;
  }

  // Set the CLIPBOARD_TYPE_SELECTION to the ui::Clipboard.
  ui::ScopedClipboardWriter clipboard_writer(
      ui::Clipboard::GetForCurrentThread(),
      ui::CLIPBOARD_TYPE_SELECTION);
  clipboard_writer.WriteText(text.substr(pos, n));
#endif  // defined(USE_X11) && !defined(OS_CHROMEOS)
}

void RenderWidgetHostViewAura::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  if (selection_anchor_rect_ == params.anchor_rect &&
      selection_focus_rect_ == params.focus_rect)
    return;

  selection_anchor_rect_ = params.anchor_rect;
  selection_focus_rect_ = params.focus_rect;

  if (GetInputMethod())
    GetInputMethod()->OnCaretBoundsChanged(this);

  if (touch_editing_client_) {
    touch_editing_client_->OnSelectionOrCursorChanged(selection_anchor_rect_,
        selection_focus_rect_);
  }
}

void RenderWidgetHostViewAura::ScrollOffsetChanged() {
  aura::Window* root = window_->GetRootWindow();
  if (!root)
    return;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root);
  if (cursor_client && !cursor_client->IsCursorVisible())
    cursor_client->DisableMouseEvents();
}

BackingStore* RenderWidgetHostViewAura::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStoreAura(host_, size);
}

void RenderWidgetHostViewAura::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    const SkBitmap::Config config) {
  // Only ARGB888 and RGB565 supported as of now.
  bool format_support = ((config == SkBitmap::kRGB_565_Config) ||
                         (config == SkBitmap::kARGB_8888_Config));
  if (!format_support) {
    DCHECK(format_support);
    callback.Run(false, SkBitmap());
    return;
  }
  if (!CanCopyToBitmap()) {
    callback.Run(false, SkBitmap());
    return;
  }

  const gfx::Size& dst_size_in_pixel = ConvertViewSizeToPixel(this, dst_size);
  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateRequest(base::Bind(
          &RenderWidgetHostViewAura::CopyFromCompositingSurfaceHasResult,
          dst_size_in_pixel,
          config,
          callback));
  gfx::Rect src_subrect_in_pixel =
      ConvertRectToPixel(current_device_scale_factor_, src_subrect);
  request->set_area(src_subrect_in_pixel);
  RequestCopyOfOutput(request.Pass());
}

void RenderWidgetHostViewAura::CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) {
  if (!CanCopyToVideoFrame()) {
    callback.Run(false);
    return;
  }

  // Try get a texture to reuse.
  scoped_refptr<OwnedMailbox> subscriber_texture;
  if (frame_subscriber_) {
    if (!idle_frame_subscriber_textures_.empty()) {
      subscriber_texture = idle_frame_subscriber_textures_.back();
      idle_frame_subscriber_textures_.pop_back();
    } else if (GLHelper* helper =
                   ImageTransportFactory::GetInstance()->GetGLHelper()) {
      subscriber_texture = new OwnedMailbox(helper);
    }
    if (subscriber_texture.get())
      active_frame_subscriber_textures_.insert(subscriber_texture.get());
  }

  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateRequest(base::Bind(
          &RenderWidgetHostViewAura::
               CopyFromCompositingSurfaceHasResultForVideo,
          AsWeakPtr(),  // For caching the ReadbackYUVInterface on this class.
          subscriber_texture,
          target,
          callback));
  gfx::Rect src_subrect_in_pixel =
      ConvertRectToPixel(current_device_scale_factor_, src_subrect);
  request->set_area(src_subrect_in_pixel);
  if (subscriber_texture.get()) {
    request->SetTextureMailbox(
        cc::TextureMailbox(subscriber_texture->mailbox(),
                           subscriber_texture->target(),
                           subscriber_texture->sync_point()));
  }
  RequestCopyOfOutput(request.Pass());
}

bool RenderWidgetHostViewAura::CanCopyToBitmap() const {
  return GetCompositor() && window_->layer()->has_external_content();
}

bool RenderWidgetHostViewAura::CanCopyToVideoFrame() const {
  return GetCompositor() &&
         window_->layer()->has_external_content() &&
         host_->is_accelerated_compositing_active();
}

bool RenderWidgetHostViewAura::CanSubscribeFrame() const {
  return true;
}

void RenderWidgetHostViewAura::BeginFrameSubscription(
    scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) {
  frame_subscriber_ = subscriber.Pass();
}

void RenderWidgetHostViewAura::EndFrameSubscription() {
  idle_frame_subscriber_textures_.clear();
  frame_subscriber_.reset();
}

void RenderWidgetHostViewAura::OnAcceleratedCompositingStateChange() {
}

void RenderWidgetHostViewAura::AcceleratedSurfaceInitialized(int host_id,
                                                             int route_id) {
}

bool RenderWidgetHostViewAura::ShouldSkipFrame(gfx::Size size_in_dip) const {
  if (can_lock_compositor_ == NO_PENDING_RENDERER_FRAME ||
      can_lock_compositor_ == NO_PENDING_COMMIT ||
      !resize_lock_.get())
    return false;

  return size_in_dip != resize_lock_->expected_size();
}

void RenderWidgetHostViewAura::InternalSetBounds(const gfx::Rect& rect) {
  if (HasDisplayPropertyChanged(window_))
    host_->InvalidateScreenInfo();

  // Don't recursively call SetBounds if this bounds update is the result of
  // a Window::SetBoundsInternal call.
  if (!in_bounds_changed_)
    window_->SetBounds(rect);
  host_->WasResized();
  MaybeCreateResizeLock();
  if (touch_editing_client_) {
    touch_editing_client_->OnSelectionOrCursorChanged(selection_anchor_rect_,
      selection_focus_rect_);
  }
#if defined(OS_WIN)
  // Create the legacy dummy window which corresponds to the bounds of the
  // webcontents. This will be passed as the container window for windowless
  // plugins.
  // Plugins like Flash assume the container window which is returned via the
  // NPNVnetscapeWindow property corresponds to the bounds of the webpage.
  // This is not true in Aura where we have only HWND which is the main Aura
  // window. If we return this window to plugins like Flash then it causes the
  // coordinate translations done by these plugins to break.
  // Additonally the legacy dummy window is needed for accessibility and for
  // scrolling to work in legacy drivers for trackpoints/trackpads, etc.
  if (GetNativeViewId()) {
    if (!legacy_render_widget_host_HWND_) {
      legacy_render_widget_host_HWND_ = LegacyRenderWidgetHostHWND::Create(
          reinterpret_cast<HWND>(GetNativeViewId()));
    }
    if (legacy_render_widget_host_HWND_) {
      legacy_render_widget_host_HWND_->SetBounds(
          window_->GetBoundsInRootWindow());
    }
  }

  if (mouse_locked_)
    UpdateMouseLockRegion();
#endif
}

void RenderWidgetHostViewAura::CheckResizeLock() {
  if (!resize_lock_ ||
      resize_lock_->expected_size() != current_frame_size_in_dip_)
    return;

  // Since we got the size we were looking for, unlock the compositor. But delay
  // the release of the lock until we've kicked a frame with the new texture, to
  // avoid resizing the UI before we have a chance to draw a "good" frame.
  resize_lock_->UnlockCompositor();
  ui::Compositor* compositor = GetCompositor();
  if (compositor) {
    if (!compositor->HasObserver(this))
      compositor->AddObserver(this);
  }
}

void RenderWidgetHostViewAura::DidReceiveFrameFromRenderer() {
  if (frame_subscriber() && CanCopyToVideoFrame()) {
    const base::TimeTicks present_time = base::TimeTicks::Now();
    scoped_refptr<media::VideoFrame> frame;
    RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback callback;
    if (frame_subscriber()->ShouldCaptureFrame(present_time,
                                               &frame, &callback)) {
      CopyFromCompositingSurfaceToVideoFrame(
          gfx::Rect(current_frame_size_in_dip_),
          frame,
          base::Bind(callback, present_time));
    }
  }
}

#if defined(OS_WIN)
void RenderWidgetHostViewAura::UpdateConstrainedWindowRects(
    const std::vector<gfx::Rect>& rects) {
  // Check this before setting constrained_rects_, so that next time they're set
  // and we have a root window we don't early return.
  if (!window_->GetHost())
    return;

  if (rects == constrained_rects_)
    return;

  constrained_rects_ = rects;

  HWND parent = window_->GetHost()->GetAcceleratedWidget();
  CutoutRectsParams params;
  params.widget = this;
  params.cutout_rects = constrained_rects_;
  params.geometry = &plugin_window_moves_;
  LPARAM lparam = reinterpret_cast<LPARAM>(&params);
  EnumChildWindows(parent, SetCutoutRectsCallback, lparam);
}

void RenderWidgetHostViewAura::UpdateMouseLockRegion() {
  // Clip the cursor if chrome is running on regular desktop.
  if (gfx::Screen::GetScreenFor(window_) == gfx::Screen::GetNativeScreen()) {
    RECT window_rect = window_->GetBoundsInScreen().ToRECT();
    ::ClipCursor(&window_rect);
  }
}
#endif

void RenderWidgetHostViewAura::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params_in_pixel,
    int gpu_host_id) {
  // Oldschool composited mode is no longer supported.
}

void RenderWidgetHostViewAura::SwapDelegatedFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::DelegatedFrameData> frame_data,
    float frame_device_scale_factor,
    const std::vector<ui::LatencyInfo>& latency_info) {
  DCHECK_NE(0u, frame_data->render_pass_list.size());

  cc::RenderPass* root_pass = frame_data->render_pass_list.back();

  gfx::Size frame_size = root_pass->output_rect.size();
  gfx::Size frame_size_in_dip =
      ConvertSizeToDIP(frame_device_scale_factor, frame_size);

  gfx::Rect damage_rect = gfx::ToEnclosingRect(root_pass->damage_rect);
  damage_rect.Intersect(gfx::Rect(frame_size));
  gfx::Rect damage_rect_in_dip =
      ConvertRectToDIP(frame_device_scale_factor, damage_rect);

  if (ShouldSkipFrame(frame_size_in_dip)) {
    cc::CompositorFrameAck ack;
    cc::TransferableResource::ReturnResources(frame_data->resource_list,
                                              &ack.resources);
    RenderWidgetHostImpl::SendSwapCompositorFrameAck(
        host_->GetRoutingID(), output_surface_id,
        host_->GetProcess()->GetID(), ack);
    skipped_frames_ = true;
    return;
  }

  if (skipped_frames_) {
    skipped_frames_ = false;
    damage_rect = gfx::Rect(frame_size);
    damage_rect_in_dip = gfx::Rect(frame_size_in_dip);

    // Give the same damage rect to the compositor.
    cc::RenderPass* root_pass = frame_data->render_pass_list.back();
    root_pass->damage_rect = damage_rect;
  }

  if (output_surface_id != last_output_surface_id_) {
    // Resource ids are scoped by the output surface.
    // If the originating output surface doesn't match the last one, it
    // indicates the renderer's output surface may have been recreated, in which
    // case we should recreate the DelegatedRendererLayer, to avoid matching
    // resources from the old one with resources from the new one which would
    // have the same id. Changing the layer to showing painted content destroys
    // the DelegatedRendererLayer.
    EvictDelegatedFrame();

    // Drop the cc::DelegatedFrameResourceCollection so that we will not return
    // any resources from the old output surface with the new output surface id.
    if (resource_collection_.get()) {
      resource_collection_->SetClient(NULL);

      if (resource_collection_->LoseAllResources())
        SendReturnedDelegatedResources(last_output_surface_id_);

      resource_collection_ = NULL;
    }
    last_output_surface_id_ = output_surface_id;
  }
  if (frame_size.IsEmpty()) {
    DCHECK_EQ(0u, frame_data->resource_list.size());
    EvictDelegatedFrame();
  } else {
    if (!resource_collection_) {
      resource_collection_ = new cc::DelegatedFrameResourceCollection;
      resource_collection_->SetClient(this);
    }
    // If the physical frame size changes, we need a new |frame_provider_|. If
    // the physical frame size is the same, but the size in DIP changed, we
    // need to adjust the scale at which the frames will be drawn, and we do
    // this by making a new |frame_provider_| also to ensure the scale change
    // is presented in sync with the new frame content.
    if (!frame_provider_.get() || frame_size != frame_provider_->frame_size() ||
        frame_size_in_dip != current_frame_size_in_dip_) {
      frame_provider_ = new cc::DelegatedFrameProvider(
          resource_collection_.get(), frame_data.Pass());
      window_->layer()->SetShowDelegatedContent(frame_provider_.get(),
                                                frame_size_in_dip);
    } else {
      frame_provider_->SetFrameData(frame_data.Pass());
    }
  }
  released_front_lock_ = NULL;
  current_frame_size_in_dip_ = frame_size_in_dip;
  CheckResizeLock();

  window_->SchedulePaintInRect(damage_rect_in_dip);

  pending_delegated_ack_count_++;

  ui::Compositor* compositor = GetCompositor();
  if (!compositor) {
    SendDelegatedFrameAck(output_surface_id);
  } else {
    for (size_t i = 0; i < latency_info.size(); i++)
      compositor->SetLatencyInfo(latency_info[i]);
    AddOnCommitCallbackAndDisableLocks(
        base::Bind(&RenderWidgetHostViewAura::SendDelegatedFrameAck,
                   AsWeakPtr(),
                   output_surface_id));
  }
  DidReceiveFrameFromRenderer();
  if (frame_provider_.get())
    delegated_frame_evictor_->SwappedFrame(!host_->is_hidden());
  // Note: the frame may have been evicted immediately.
}

void RenderWidgetHostViewAura::SendDelegatedFrameAck(uint32 output_surface_id) {
  cc::CompositorFrameAck ack;
  if (resource_collection_)
    resource_collection_->TakeUnusedResourcesForChildCompositor(&ack.resources);
  RenderWidgetHostImpl::SendSwapCompositorFrameAck(host_->GetRoutingID(),
                                                   output_surface_id,
                                                   host_->GetProcess()->GetID(),
                                                   ack);
  DCHECK_GT(pending_delegated_ack_count_, 0);
  pending_delegated_ack_count_--;
}

void RenderWidgetHostViewAura::UnusedResourcesAreAvailable() {
  if (pending_delegated_ack_count_)
    return;

  SendReturnedDelegatedResources(last_output_surface_id_);
}

void RenderWidgetHostViewAura::SendReturnedDelegatedResources(
    uint32 output_surface_id) {
  DCHECK(resource_collection_);

  cc::CompositorFrameAck ack;
  resource_collection_->TakeUnusedResourcesForChildCompositor(&ack.resources);
  DCHECK(!ack.resources.empty());

  RenderWidgetHostImpl::SendReclaimCompositorResources(
      host_->GetRoutingID(),
      output_surface_id,
      host_->GetProcess()->GetID(),
      ack);
}

void RenderWidgetHostViewAura::EvictDelegatedFrame() {
  window_->layer()->SetShowPaintedContent();
  frame_provider_ = NULL;
  delegated_frame_evictor_->DiscardedFrame();
}

void RenderWidgetHostViewAura::OnSwapCompositorFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  TRACE_EVENT0("content", "RenderWidgetHostViewAura::OnSwapCompositorFrame");
  if (frame->delegated_frame_data) {
    SwapDelegatedFrame(output_surface_id,
                       frame->delegated_frame_data.Pass(),
                       frame->metadata.device_scale_factor,
                       frame->metadata.latency_info);
    return;
  }

  if (frame->software_frame_data) {
    DLOG(ERROR) << "Unable to use software frame in aura";
    RecordAction(
        base::UserMetricsAction("BadMessageTerminate_SharedMemoryAura"));
    host_->GetProcess()->ReceivedBadMessage();
    return;
  }
}

#if defined(OS_WIN)
void RenderWidgetHostViewAura::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {
  if (GetBrowserAccessibilityManager()) {
    GetBrowserAccessibilityManager()->ToBrowserAccessibilityManagerWin()
        ->set_parent_iaccessible(accessible_parent);
  }
}

gfx::NativeViewId RenderWidgetHostViewAura::GetParentForWindowlessPlugin()
    const {
  if (legacy_render_widget_host_HWND_) {
    return reinterpret_cast<gfx::NativeViewId>(
        legacy_render_widget_host_HWND_->hwnd());
  }
  return NULL;
}
#endif

void RenderWidgetHostViewAura::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params_in_pixel,
    int gpu_host_id) {
  // Oldschool composited mode is no longer supported.
}

void RenderWidgetHostViewAura::AcceleratedSurfaceSuspend() {
}

void RenderWidgetHostViewAura::AcceleratedSurfaceRelease() {
}

bool RenderWidgetHostViewAura::HasAcceleratedSurface(
    const gfx::Size& desired_size) {
  // Aura doesn't use GetBackingStore for accelerated pages, so it doesn't
  // matter what is returned here as GetBackingStore is the only caller of this
  // method. TODO(jbates) implement this if other Aura code needs it.
  return false;
}

// static
void RenderWidgetHostViewAura::CopyFromCompositingSurfaceHasResult(
    const gfx::Size& dst_size_in_pixel,
    const SkBitmap::Config config,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  if (result->IsEmpty() || result->size().IsEmpty()) {
    callback.Run(false, SkBitmap());
    return;
  }

  if (result->HasTexture()) {
    PrepareTextureCopyOutputResult(dst_size_in_pixel, config,
                                   callback,
                                   result.Pass());
    return;
  }

  DCHECK(result->HasBitmap());
  PrepareBitmapCopyOutputResult(dst_size_in_pixel, config, callback,
                                result.Pass());
}

static void CopyFromCompositingSurfaceFinished(
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    scoped_ptr<SkBitmap> bitmap,
    scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock,
    bool result) {
  bitmap_pixels_lock.reset();

  uint32 sync_point = 0;
  if (result) {
    GLHelper* gl_helper = ImageTransportFactory::GetInstance()->GetGLHelper();
    sync_point = gl_helper->InsertSyncPoint();
  }
  bool lost_resource = sync_point == 0;
  release_callback->Run(sync_point, lost_resource);

  callback.Run(result, *bitmap);
}

// static
void RenderWidgetHostViewAura::PrepareTextureCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    const SkBitmap::Config config,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  DCHECK(result->HasTexture());
  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(callback, false, SkBitmap()));

  scoped_ptr<SkBitmap> bitmap(new SkBitmap);
  bitmap->setConfig(config,
                    dst_size_in_pixel.width(), dst_size_in_pixel.height(),
                    0, kOpaque_SkAlphaType);
  if (!bitmap->allocPixels())
    return;

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;

  scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock(
      new SkAutoLockPixels(*bitmap));
  uint8* pixels = static_cast<uint8*>(bitmap->getPixels());

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());
  if (!texture_mailbox.IsTexture())
    return;

  ignore_result(scoped_callback_runner.Release());

  gl_helper->CropScaleReadbackAndCleanMailbox(
      texture_mailbox.mailbox(),
      texture_mailbox.sync_point(),
      result->size(),
      gfx::Rect(result->size()),
      dst_size_in_pixel,
      pixels,
      config,
      base::Bind(&CopyFromCompositingSurfaceFinished,
                 callback,
                 base::Passed(&release_callback),
                 base::Passed(&bitmap),
                 base::Passed(&bitmap_pixels_lock)));
}

// static
void RenderWidgetHostViewAura::PrepareBitmapCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    const SkBitmap::Config config,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  if (config != SkBitmap::kARGB_8888_Config) {
    NOTIMPLEMENTED();
    callback.Run(false, SkBitmap());
    return;
  }
  DCHECK(result->HasBitmap());
  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(callback, false, SkBitmap()));

  scoped_ptr<SkBitmap> source = result->TakeBitmap();
  DCHECK(source);
  if (!source)
    return;

  ignore_result(scoped_callback_runner.Release());

  SkBitmap bitmap = skia::ImageOperations::Resize(
      *source,
      skia::ImageOperations::RESIZE_BEST,
      dst_size_in_pixel.width(),
      dst_size_in_pixel.height());
  callback.Run(true, bitmap);
}

// static
void RenderWidgetHostViewAura::ReturnSubscriberTexture(
    base::WeakPtr<RenderWidgetHostViewAura> rwhva,
    scoped_refptr<OwnedMailbox> subscriber_texture,
    uint32 sync_point) {
  if (!subscriber_texture.get())
    return;
  if (!rwhva)
    return;
  DCHECK_NE(
      rwhva->active_frame_subscriber_textures_.count(subscriber_texture.get()),
      0u);

  subscriber_texture->UpdateSyncPoint(sync_point);

  rwhva->active_frame_subscriber_textures_.erase(subscriber_texture.get());
  if (rwhva->frame_subscriber_ && subscriber_texture->texture_id())
    rwhva->idle_frame_subscriber_textures_.push_back(subscriber_texture);
}

void RenderWidgetHostViewAura::CopyFromCompositingSurfaceFinishedForVideo(
    base::WeakPtr<RenderWidgetHostViewAura> rwhva,
    const base::Callback<void(bool)>& callback,
    scoped_refptr<OwnedMailbox> subscriber_texture,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    bool result) {
  callback.Run(result);

  uint32 sync_point = 0;
  if (result) {
    GLHelper* gl_helper = ImageTransportFactory::GetInstance()->GetGLHelper();
    sync_point = gl_helper->InsertSyncPoint();
  }
  if (release_callback) {
    // A release callback means the texture came from the compositor, so there
    // should be no |subscriber_texture|.
    DCHECK(!subscriber_texture);
    bool lost_resource = sync_point == 0;
    release_callback->Run(sync_point, lost_resource);
  }
  ReturnSubscriberTexture(rwhva, subscriber_texture, sync_point);
}

// static
void RenderWidgetHostViewAura::CopyFromCompositingSurfaceHasResultForVideo(
    base::WeakPtr<RenderWidgetHostViewAura> rwhva,
    scoped_refptr<OwnedMailbox> subscriber_texture,
    scoped_refptr<media::VideoFrame> video_frame,
    const base::Callback<void(bool)>& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  base::ScopedClosureRunner scoped_callback_runner(base::Bind(callback, false));
  base::ScopedClosureRunner scoped_return_subscriber_texture(
      base::Bind(&ReturnSubscriberTexture, rwhva, subscriber_texture, 0));

  if (!rwhva)
    return;
  if (result->IsEmpty())
    return;
  if (result->size().IsEmpty())
    return;

  // Compute the dest size we want after the letterboxing resize. Make the
  // coordinates and sizes even because we letterbox in YUV space
  // (see CopyRGBToVideoFrame). They need to be even for the UV samples to
  // line up correctly.
  // The video frame's coded_size() and the result's size() are both physical
  // pixels.
  gfx::Rect region_in_frame =
      media::ComputeLetterboxRegion(gfx::Rect(video_frame->coded_size()),
                                    result->size());
  region_in_frame = gfx::Rect(region_in_frame.x() & ~1,
                              region_in_frame.y() & ~1,
                              region_in_frame.width() & ~1,
                              region_in_frame.height() & ~1);
  if (region_in_frame.IsEmpty())
    return;

  if (!result->HasTexture()) {
    DCHECK(result->HasBitmap());
    scoped_ptr<SkBitmap> bitmap = result->TakeBitmap();
    // Scale the bitmap to the required size, if necessary.
    SkBitmap scaled_bitmap;
    if (result->size().width() != region_in_frame.width() ||
        result->size().height() != region_in_frame.height()) {
      skia::ImageOperations::ResizeMethod method =
          skia::ImageOperations::RESIZE_GOOD;
      scaled_bitmap = skia::ImageOperations::Resize(*bitmap.get(), method,
                                                    region_in_frame.width(),
                                                    region_in_frame.height());
    } else {
      scaled_bitmap = *bitmap.get();
    }

    {
      SkAutoLockPixels scaled_bitmap_locker(scaled_bitmap);

      media::CopyRGBToVideoFrame(
          reinterpret_cast<uint8*>(scaled_bitmap.getPixels()),
          scaled_bitmap.rowBytes(),
          region_in_frame,
          video_frame.get());
    }
    ignore_result(scoped_callback_runner.Release());
    callback.Run(true);
    return;
  }

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;
  if (subscriber_texture.get() && !subscriber_texture->texture_id())
    return;

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());
  if (!texture_mailbox.IsTexture())
    return;

  gfx::Rect result_rect(result->size());

  content::ReadbackYUVInterface* yuv_readback_pipeline =
      rwhva->yuv_readback_pipeline_.get();
  if (yuv_readback_pipeline == NULL ||
      yuv_readback_pipeline->scaler()->SrcSize() != result_rect.size() ||
      yuv_readback_pipeline->scaler()->SrcSubrect() != result_rect ||
      yuv_readback_pipeline->scaler()->DstSize() != region_in_frame.size()) {
    GLHelper::ScalerQuality quality = GLHelper::SCALER_QUALITY_FAST;
    std::string quality_switch = switches::kTabCaptureDownscaleQuality;
    // If we're scaling up, we can use the "best" quality.
    if (result_rect.size().width() < region_in_frame.size().width() &&
        result_rect.size().height() < region_in_frame.size().height())
      quality_switch = switches::kTabCaptureUpscaleQuality;

    std::string switch_value =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(quality_switch);
    if (switch_value == "fast")
      quality = GLHelper::SCALER_QUALITY_FAST;
    else if (switch_value == "good")
      quality = GLHelper::SCALER_QUALITY_GOOD;
    else if (switch_value == "best")
      quality = GLHelper::SCALER_QUALITY_BEST;

    rwhva->yuv_readback_pipeline_.reset(
        gl_helper->CreateReadbackPipelineYUV(quality,
                                             result_rect.size(),
                                             result_rect,
                                             video_frame->coded_size(),
                                             region_in_frame,
                                             true,
                                             true));
    yuv_readback_pipeline = rwhva->yuv_readback_pipeline_.get();
  }

  ignore_result(scoped_callback_runner.Release());
  ignore_result(scoped_return_subscriber_texture.Release());
  base::Callback<void(bool result)> finished_callback = base::Bind(
      &RenderWidgetHostViewAura::CopyFromCompositingSurfaceFinishedForVideo,
      rwhva->AsWeakPtr(),
      callback,
      subscriber_texture,
      base::Passed(&release_callback));
  yuv_readback_pipeline->ReadbackYUV(texture_mailbox.mailbox(),
                                     texture_mailbox.sync_point(),
                                     video_frame.get(),
                                     finished_callback);
}

void RenderWidgetHostViewAura::GetScreenInfo(WebScreenInfo* results) {
  GetScreenInfoForWindow(results, window_->GetRootWindow() ? window_ : NULL);
}

gfx::Rect RenderWidgetHostViewAura::GetBoundsInRootWindow() {
#if defined(OS_WIN)
  // aura::Window::GetBoundsInScreen doesn't take non-client area into
  // account.
  RECT window_rect = {0};

  aura::Window* top_level = window_->GetToplevelWindow();
  aura::WindowTreeHost* host = top_level->GetHost();
  if (!host)
    return top_level->GetBoundsInScreen();
  HWND hwnd = host->GetAcceleratedWidget();
  ::GetWindowRect(hwnd, &window_rect);
  gfx::Rect rect(window_rect);

  // Maximized windows are outdented from the work area by the frame thickness
  // even though this "frame" is not painted.  This confuses code (and people)
  // that think of a maximized window as corresponding exactly to the work area.
  // Correct for this by subtracting the frame thickness back off.
  if (::IsZoomed(hwnd)) {
    rect.Inset(GetSystemMetrics(SM_CXSIZEFRAME),
               GetSystemMetrics(SM_CYSIZEFRAME));
  }

  return gfx::win::ScreenToDIPRect(rect);
#else
  return window_->GetToplevelWindow()->GetBoundsInScreen();
#endif
}

void RenderWidgetHostViewAura::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  if (touch_editing_client_)
    touch_editing_client_->GestureEventAck(event.type);
}

void RenderWidgetHostViewAura::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch, InputEventAckState ack_result) {
  ScopedVector<ui::TouchEvent> events;
  if (!MakeUITouchEventsFromWebTouchEvents(touch, &events,
                                           SCREEN_COORDINATES))
    return;

  aura::WindowTreeHost* host = window_->GetHost();
  // |host| is NULL during tests.
  if (!host)
    return;

  ui::EventResult result = (ack_result ==
      INPUT_EVENT_ACK_STATE_CONSUMED) ? ui::ER_HANDLED : ui::ER_UNHANDLED;
  for (ScopedVector<ui::TouchEvent>::iterator iter = events.begin(),
      end = events.end(); iter != end; ++iter) {
    host->dispatcher()->ProcessedTouchEvent((*iter), window_, result);
  }
}

scoped_ptr<SyntheticGestureTarget>
RenderWidgetHostViewAura::CreateSyntheticGestureTarget() {
  return scoped_ptr<SyntheticGestureTarget>(
      new SyntheticGestureTargetAura(host_));
}

void RenderWidgetHostViewAura::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
  // Not needed. Mac-only.
}

void RenderWidgetHostViewAura::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  // Not needed. Mac-only.
}

void RenderWidgetHostViewAura::CreateBrowserAccessibilityManagerIfNeeded() {
  if (GetBrowserAccessibilityManager())
    return;

  BrowserAccessibilityManager* manager = NULL;
#if defined(OS_WIN)
  aura::WindowTreeHost* host = window_->GetHost();
  if (!host)
    return;
  HWND hwnd = host->GetAcceleratedWidget();

  // The accessible_parent may be NULL at this point. The WebContents will pass
  // it down to this instance (by way of the RenderViewHost and
  // RenderWidgetHost) when it is known. This instance will then set it on its
  // BrowserAccessibilityManager.
  gfx::NativeViewAccessible accessible_parent =
      host_->GetParentNativeViewAccessible();

  if (legacy_render_widget_host_HWND_) {
    manager = new BrowserAccessibilityManagerWin(
        legacy_render_widget_host_HWND_.get(), accessible_parent,
        BrowserAccessibilityManagerWin::GetEmptyDocument(), this);
  }
#else
  manager = BrowserAccessibilityManager::Create(
      BrowserAccessibilityManager::GetEmptyDocument(), this);
#endif
  SetBrowserAccessibilityManager(manager);
}

gfx::GLSurfaceHandle RenderWidgetHostViewAura::GetCompositingSurface() {
  return ImageTransportFactory::GetInstance()->GetSharedSurfaceHandle();
}

bool RenderWidgetHostViewAura::LockMouse() {
  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window)
    return false;

  if (mouse_locked_)
    return true;

  mouse_locked_ = true;
#if !defined(OS_WIN)
  window_->SetCapture();
#else
  UpdateMouseLockRegion();
#endif
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client) {
    cursor_client->HideCursor();
    cursor_client->LockCursor();
  }

  if (ShouldMoveToCenter()) {
    synthetic_move_sent_ = true;
    window_->MoveCursorTo(gfx::Rect(window_->bounds().size()).CenterPoint());
  }
  tooltip_disabler_.reset(new aura::client::ScopedTooltipDisabler(root_window));
  return true;
}

void RenderWidgetHostViewAura::UnlockMouse() {
  tooltip_disabler_.reset();

  aura::Window* root_window = window_->GetRootWindow();
  if (!mouse_locked_ || !root_window)
    return;

  mouse_locked_ = false;

#if !defined(OS_WIN)
  window_->ReleaseCapture();
#else
  ::ClipCursor(NULL);
#endif
  window_->MoveCursorTo(unlocked_mouse_position_);
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client) {
    cursor_client->UnlockCursor();
    cursor_client->ShowCursor();
  }

  host_->LostMouseLock();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::TextInputClient implementation:
void RenderWidgetHostViewAura::SetCompositionText(
    const ui::CompositionText& composition) {
  if (!host_)
    return;

  // ui::CompositionUnderline should be identical to
  // blink::WebCompositionUnderline, so that we can do reinterpret_cast safely.
  COMPILE_ASSERT(sizeof(ui::CompositionUnderline) ==
                 sizeof(blink::WebCompositionUnderline),
                 ui_CompositionUnderline__WebKit_WebCompositionUnderline_diff);

  // TODO(suzhe): convert both renderer_host and renderer to use
  // ui::CompositionText.
  const std::vector<blink::WebCompositionUnderline>& underlines =
      reinterpret_cast<const std::vector<blink::WebCompositionUnderline>&>(
          composition.underlines);

  // TODO(suzhe): due to a bug of webkit, we can't use selection range with
  // composition string. See: https://bugs.webkit.org/show_bug.cgi?id=37788
  host_->ImeSetComposition(composition.text, underlines,
                           composition.selection.end(),
                           composition.selection.end());

  has_composition_text_ = !composition.text.empty();
}

void RenderWidgetHostViewAura::ConfirmCompositionText() {
  if (host_ && has_composition_text_) {
    host_->ImeConfirmComposition(base::string16(), gfx::Range::InvalidRange(),
                                 false);
  }
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::ClearCompositionText() {
  if (host_ && has_composition_text_)
    host_->ImeCancelComposition();
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::InsertText(const base::string16& text) {
  DCHECK(text_input_type_ != ui::TEXT_INPUT_TYPE_NONE);
  if (host_)
    host_->ImeConfirmComposition(text, gfx::Range::InvalidRange(), false);
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::InsertChar(base::char16 ch, int flags) {
  if (popup_child_host_view_ && popup_child_host_view_->NeedsInputGrab()) {
    popup_child_host_view_->InsertChar(ch, flags);
    return;
  }

  // Ignore character messages for VKEY_RETURN sent on CTRL+M. crbug.com/315547
  if (host_ && (accept_return_character_ || ch != ui::VKEY_RETURN)) {
    double now = ui::EventTimeForNow().InSecondsF();
    // Send a blink::WebInputEvent::Char event to |host_|.
    NativeWebKeyboardEvent webkit_event(ui::ET_KEY_PRESSED,
                                        true /* is_char */,
                                        ch,
                                        flags,
                                        now);
    ForwardKeyboardEvent(webkit_event);
  }
}

gfx::NativeWindow RenderWidgetHostViewAura::GetAttachedWindow() const {
  return window_;
}

ui::TextInputType RenderWidgetHostViewAura::GetTextInputType() const {
  return text_input_type_;
}

ui::TextInputMode RenderWidgetHostViewAura::GetTextInputMode() const {
  return text_input_mode_;
}

bool RenderWidgetHostViewAura::CanComposeInline() const {
  return can_compose_inline_;
}

gfx::Rect RenderWidgetHostViewAura::ConvertRectToScreen(
    const gfx::Rect& rect) const {
  gfx::Point origin = rect.origin();
  gfx::Point end = gfx::Point(rect.right(), rect.bottom());

  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window)
    return rect;
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  if (!screen_position_client)
    return rect;
  screen_position_client->ConvertPointToScreen(window_, &origin);
  screen_position_client->ConvertPointToScreen(window_, &end);
  return gfx::Rect(origin.x(),
                   origin.y(),
                   end.x() - origin.x(),
                   end.y() - origin.y());
}

gfx::Rect RenderWidgetHostViewAura::ConvertRectFromScreen(
    const gfx::Rect& rect) const {
  gfx::Point origin = rect.origin();
  gfx::Point end = gfx::Point(rect.right(), rect.bottom());

  aura::Window* root_window = window_->GetRootWindow();
  if (root_window) {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root_window);
    screen_position_client->ConvertPointFromScreen(window_, &origin);
    screen_position_client->ConvertPointFromScreen(window_, &end);
    return gfx::Rect(origin.x(),
                     origin.y(),
                     end.x() - origin.x(),
                     end.y() - origin.y());
  }

  return rect;
}

gfx::Rect RenderWidgetHostViewAura::GetCaretBounds() const {
  const gfx::Rect rect =
      gfx::UnionRects(selection_anchor_rect_, selection_focus_rect_);
  return ConvertRectToScreen(rect);
}

bool RenderWidgetHostViewAura::GetCompositionCharacterBounds(
    uint32 index,
    gfx::Rect* rect) const {
  DCHECK(rect);
  if (index >= composition_character_bounds_.size())
    return false;
  *rect = ConvertRectToScreen(composition_character_bounds_[index]);
  return true;
}

bool RenderWidgetHostViewAura::HasCompositionText() const {
  return has_composition_text_;
}

bool RenderWidgetHostViewAura::GetTextRange(gfx::Range* range) const {
  range->set_start(selection_text_offset_);
  range->set_end(selection_text_offset_ + selection_text_.length());
  return true;
}

bool RenderWidgetHostViewAura::GetCompositionTextRange(
    gfx::Range* range) const {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewAura::GetSelectionRange(gfx::Range* range) const {
  range->set_start(selection_range_.start());
  range->set_end(selection_range_.end());
  return true;
}

bool RenderWidgetHostViewAura::SetSelectionRange(const gfx::Range& range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewAura::DeleteRange(const gfx::Range& range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewAura::GetTextFromRange(
    const gfx::Range& range,
    base::string16* text) const {
  gfx::Range selection_text_range(selection_text_offset_,
      selection_text_offset_ + selection_text_.length());

  if (!selection_text_range.Contains(range)) {
    text->clear();
    return false;
  }
  if (selection_text_range.EqualsIgnoringDirection(range)) {
    // Avoid calling substr whose performance is low.
    *text = selection_text_;
  } else {
    *text = selection_text_.substr(
        range.GetMin() - selection_text_offset_,
        range.length());
  }
  return true;
}

void RenderWidgetHostViewAura::OnInputMethodChanged() {
  if (!host_)
    return;

  if (GetInputMethod())
    host_->SetInputMethodActive(GetInputMethod()->IsActive());

  // TODO(suzhe): implement the newly added “locale” property of HTML DOM
  // TextEvent.
}

bool RenderWidgetHostViewAura::ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) {
  if (!host_)
    return false;
  host_->UpdateTextDirection(
      direction == base::i18n::RIGHT_TO_LEFT ?
      blink::WebTextDirectionRightToLeft :
      blink::WebTextDirectionLeftToRight);
  host_->NotifyTextDirection();
  return true;
}

void RenderWidgetHostViewAura::ExtendSelectionAndDelete(
    size_t before, size_t after) {
  RenderFrameHostImpl* rfh = GetFocusedFrame();
  if (rfh)
    rfh->ExtendSelectionAndDelete(before, after);
}

void RenderWidgetHostViewAura::EnsureCaretInRect(const gfx::Rect& rect) {
  gfx::Rect intersected_rect(
      gfx::IntersectRects(rect, window_->GetBoundsInScreen()));

  if (intersected_rect.IsEmpty())
    return;

  host_->ScrollFocusedEditableNodeIntoRect(
      ConvertRectFromScreen(intersected_rect));
}

void RenderWidgetHostViewAura::OnCandidateWindowShown() {
  host_->CandidateWindowShown();
}

void RenderWidgetHostViewAura::OnCandidateWindowUpdated() {
  host_->CandidateWindowUpdated();
}

void RenderWidgetHostViewAura::OnCandidateWindowHidden() {
  host_->CandidateWindowHidden();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, gfx::DisplayObserver implementation:

void RenderWidgetHostViewAura::OnDisplayBoundsChanged(
    const gfx::Display& display) {
  gfx::Screen* screen = gfx::Screen::GetScreenFor(window_);
  if (display.id() == screen->GetDisplayNearestWindow(window_).id()) {
    UpdateScreenInfo(window_);
    current_cursor_.SetDisplayInfo(display);
    UpdateCursorIfOverSelf();
  }
}

void RenderWidgetHostViewAura::OnDisplayAdded(
    const gfx::Display& new_display) {
}

void RenderWidgetHostViewAura::OnDisplayRemoved(
    const gfx::Display& old_display) {
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::WindowDelegate implementation:

gfx::Size RenderWidgetHostViewAura::GetMinimumSize() const {
  return gfx::Size();
}

gfx::Size RenderWidgetHostViewAura::GetMaximumSize() const {
  return gfx::Size();
}

void RenderWidgetHostViewAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                               const gfx::Rect& new_bounds) {
  base::AutoReset<bool> in_bounds_changed(&in_bounds_changed_, true);
  // We care about this whenever RenderWidgetHostViewAura is not owned by a
  // WebContentsViewAura since changes to the Window's bounds need to be
  // messaged to the renderer.  WebContentsViewAura invokes SetSize() or
  // SetBounds() itself.  No matter how we got here, any redundant calls are
  // harmless.
  SetSize(new_bounds.size());
}

gfx::NativeCursor RenderWidgetHostViewAura::GetCursor(const gfx::Point& point) {
  if (mouse_locked_)
    return ui::kCursorNone;
  return current_cursor_.GetNativeCursor();
}

int RenderWidgetHostViewAura::GetNonClientComponent(
    const gfx::Point& point) const {
  return HTCLIENT;
}

bool RenderWidgetHostViewAura::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return true;
}

bool RenderWidgetHostViewAura::CanFocus() {
  return popup_type_ == blink::WebPopupTypeNone;
}

void RenderWidgetHostViewAura::OnCaptureLost() {
  host_->LostCapture();
  if (touch_editing_client_)
    touch_editing_client_->EndTouchEditing(false);
}

void RenderWidgetHostViewAura::OnPaint(gfx::Canvas* canvas) {
  bool has_backing_store = !!host_->GetBackingStore(false);
  if (has_backing_store) {
    paint_canvas_ = canvas;
    BackingStoreAura* backing_store = static_cast<BackingStoreAura*>(
        host_->GetBackingStore(true));
    paint_canvas_ = NULL;
    backing_store->SkiaShowRect(gfx::Point(), canvas);

    ui::Compositor* compositor = GetCompositor();
    if (compositor) {
      for (size_t i = 0; i < software_latency_info_.size(); i++)
        compositor->SetLatencyInfo(software_latency_info_[i]);
    }
    software_latency_info_.clear();
  } else {
    // For non-opaque windows, we don't draw anything, since we depend on the
    // canvas coming from the compositor to already be initialized as
    // transparent.
    if (window_->layer()->fills_bounds_opaquely())
      canvas->DrawColor(SK_ColorWHITE);
  }
}

void RenderWidgetHostViewAura::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  if (!host_)
    return;

  BackingStoreAura* backing_store = static_cast<BackingStoreAura*>(
      host_->GetBackingStore(false));
  if (backing_store)  // NULL in hardware path.
    backing_store->ScaleFactorChanged(device_scale_factor);

  UpdateScreenInfo(window_);

  const gfx::Display display = gfx::Screen::GetScreenFor(window_)->
      GetDisplayNearestWindow(window_);
  DCHECK_EQ(device_scale_factor, display.device_scale_factor());
  current_cursor_.SetDisplayInfo(display);
}

void RenderWidgetHostViewAura::OnWindowDestroying(aura::Window* window) {
#if defined(OS_WIN)
  HWND parent = NULL;
  // If the tab was hidden and it's closed, host_->is_hidden would have been
  // reset to false in RenderWidgetHostImpl::RendererExited.
  if (!window_->GetRootWindow() || host_->is_hidden()) {
    parent = ui::GetHiddenWindow();
  } else {
    parent = window_->GetHost()->GetAcceleratedWidget();
  }
  LPARAM lparam = reinterpret_cast<LPARAM>(this);
  EnumChildWindows(parent, WindowDestroyingCallback, lparam);
#endif

  // Make sure that the input method no longer references to this object before
  // this object is removed from the root window (i.e. this object loses access
  // to the input method).
  ui::InputMethod* input_method = GetInputMethod();
  if (input_method)
    input_method->DetachTextInputClient(this);
}

void RenderWidgetHostViewAura::OnWindowDestroyed(aura::Window* window) {
  host_->ViewDestroyed();
  delete this;
}

void RenderWidgetHostViewAura::OnWindowTargetVisibilityChanged(bool visible) {
}

bool RenderWidgetHostViewAura::HasHitTestMask() const {
  return false;
}

void RenderWidgetHostViewAura::GetHitTestMask(gfx::Path* mask) const {
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::EventHandler implementation:

void RenderWidgetHostViewAura::OnKeyEvent(ui::KeyEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewAura::OnKeyEvent");
  if (touch_editing_client_ && touch_editing_client_->HandleInputEvent(event))
    return;

  if (popup_child_host_view_ && popup_child_host_view_->NeedsInputGrab()) {
    popup_child_host_view_->OnKeyEvent(event);
    if (event->handled())
      return;
  }

  // We need to handle the Escape key for Pepper Flash.
  if (is_fullscreen_ && event->key_code() == ui::VKEY_ESCAPE) {
    // Focus the window we were created from.
    if (host_tracker_.get() && !host_tracker_->windows().empty()) {
      aura::Window* host = *(host_tracker_->windows().begin());
      aura::client::FocusClient* client = aura::client::GetFocusClient(host);
      if (client) {
        // Calling host->Focus() may delete |this|. We create a local observer
        // for that. In that case we exit without further access to any members.
        aura::WindowTracker tracker;
        aura::Window* window = window_;
        tracker.Add(window);
        host->Focus();
        if (!tracker.Contains(window)) {
          event->SetHandled();
          return;
        }
      }
    }
    if (!in_shutdown_) {
      in_shutdown_ = true;
      host_->Shutdown();
    }
  } else {
    if (event->key_code() == ui::VKEY_RETURN) {
      // Do not forward return key release events if no press event was handled.
      if (event->type() == ui::ET_KEY_RELEASED && !accept_return_character_)
        return;
      // Accept return key character events between press and release events.
      accept_return_character_ = event->type() == ui::ET_KEY_PRESSED;
    }

    // We don't have to communicate with an input method here.
    if (!event->HasNativeEvent()) {
      NativeWebKeyboardEvent webkit_event(
          event->type(),
          event->is_char(),
          event->is_char() ? event->GetCharacter() : event->key_code(),
          event->flags(),
          ui::EventTimeForNow().InSecondsF());
      ForwardKeyboardEvent(webkit_event);
    } else {
      NativeWebKeyboardEvent webkit_event(event);
      ForwardKeyboardEvent(webkit_event);
    }
  }
  event->SetHandled();
}

void RenderWidgetHostViewAura::OnMouseEvent(ui::MouseEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewAura::OnMouseEvent");

  if (touch_editing_client_ && touch_editing_client_->HandleInputEvent(event))
    return;

  if (mouse_locked_) {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(window_->GetRootWindow());
    DCHECK(!cursor_client || !cursor_client->IsCursorVisible());

    if (event->type() == ui::ET_MOUSEWHEEL) {
      blink::WebMouseWheelEvent mouse_wheel_event =
          MakeWebMouseWheelEvent(static_cast<ui::MouseWheelEvent*>(event));
      if (mouse_wheel_event.deltaX != 0 || mouse_wheel_event.deltaY != 0)
        host_->ForwardWheelEvent(mouse_wheel_event);
      return;
    }

    gfx::Point center(gfx::Rect(window_->bounds().size()).CenterPoint());

    // If we receive non client mouse messages while we are in the locked state
    // it probably means that the mouse left the borders of our window and
    // needs to be moved back to the center.
    if (event->flags() & ui::EF_IS_NON_CLIENT) {
      synthetic_move_sent_ = true;
      window_->MoveCursorTo(center);
      return;
    }

    blink::WebMouseEvent mouse_event = MakeWebMouseEvent(event);

    bool is_move_to_center_event = (event->type() == ui::ET_MOUSE_MOVED ||
        event->type() == ui::ET_MOUSE_DRAGGED) &&
        mouse_event.x == center.x() && mouse_event.y == center.y();

    ModifyEventMovementAndCoords(&mouse_event);

    bool should_not_forward = is_move_to_center_event && synthetic_move_sent_;
    if (should_not_forward) {
      synthetic_move_sent_ = false;
    } else {
      // Check if the mouse has reached the border and needs to be centered.
      if (ShouldMoveToCenter()) {
        synthetic_move_sent_ = true;
        window_->MoveCursorTo(center);
      }
      // Forward event to renderer.
      if (CanRendererHandleEvent(event) &&
          !(event->flags() & ui::EF_FROM_TOUCH)) {
        host_->ForwardMouseEvent(mouse_event);
        // Ensure that we get keyboard focus on mouse down as a plugin window
        // may have grabbed keyboard focus.
        if (event->type() == ui::ET_MOUSE_PRESSED)
          SetKeyboardFocus();
      }
    }
    return;
  }

  // As the overscroll is handled during scroll events from the trackpad, the
  // RWHVA window is transformed by the overscroll controller. This transform
  // triggers a synthetic mouse-move event to be generated (by the aura
  // RootWindow). But this event interferes with the overscroll gesture. So,
  // ignore such synthetic mouse-move events if an overscroll gesture is in
  // progress.
  if (host_->overscroll_controller() &&
      host_->overscroll_controller()->overscroll_mode() != OVERSCROLL_NONE &&
      event->flags() & ui::EF_IS_SYNTHESIZED &&
      (event->type() == ui::ET_MOUSE_ENTERED ||
       event->type() == ui::ET_MOUSE_EXITED ||
       event->type() == ui::ET_MOUSE_MOVED)) {
    event->StopPropagation();
    return;
  }

  if (event->type() == ui::ET_MOUSEWHEEL) {
#if defined(OS_WIN)
    // We get mouse wheel/scroll messages even if we are not in the foreground.
    // So here we check if we have any owned popup windows in the foreground and
    // dismiss them.
    aura::WindowTreeHost* host = window_->GetHost();
    if (host) {
      HWND parent = host->GetAcceleratedWidget();
      HWND toplevel_hwnd = ::GetAncestor(parent, GA_ROOT);
      EnumThreadWindows(GetCurrentThreadId(),
                        DismissOwnedPopups,
                        reinterpret_cast<LPARAM>(toplevel_hwnd));
    }
#endif
    blink::WebMouseWheelEvent mouse_wheel_event =
        MakeWebMouseWheelEvent(static_cast<ui::MouseWheelEvent*>(event));
    if (mouse_wheel_event.deltaX != 0 || mouse_wheel_event.deltaY != 0)
      host_->ForwardWheelEvent(mouse_wheel_event);
  } else if (CanRendererHandleEvent(event) &&
             !(event->flags() & ui::EF_FROM_TOUCH)) {
    blink::WebMouseEvent mouse_event = MakeWebMouseEvent(event);
    ModifyEventMovementAndCoords(&mouse_event);
    host_->ForwardMouseEvent(mouse_event);
    // Ensure that we get keyboard focus on mouse down as a plugin window may
    // have grabbed keyboard focus.
    if (event->type() == ui::ET_MOUSE_PRESSED)
      SetKeyboardFocus();
  }

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      window_->SetCapture();
      // Confirm existing composition text on mouse click events, to make sure
      // the input caret won't be moved with an ongoing composition text.
      FinishImeCompositionSession();
      break;
    case ui::ET_MOUSE_RELEASED:
      window_->ReleaseCapture();
      break;
    default:
      break;
  }

  // Needed to propagate mouse event to |window_->parent()->delegate()|, but
  // note that it might be something other than a WebContentsViewAura instance.
  // TODO(pkotwicz): Find a better way of doing this.
  // In fullscreen mode which is typically used by flash, don't forward
  // the mouse events to the parent. The renderer and the plugin process
  // handle these events.
  if (!is_fullscreen_ && window_->parent()->delegate() &&
      !(event->flags() & ui::EF_FROM_TOUCH)) {
    event->ConvertLocationToTarget(window_, window_->parent());
    window_->parent()->delegate()->OnMouseEvent(event);
  }

  if (!IsXButtonUpEvent(event))
    event->SetHandled();
}

void RenderWidgetHostViewAura::OnScrollEvent(ui::ScrollEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewAura::OnScrollEvent");
  if (touch_editing_client_ && touch_editing_client_->HandleInputEvent(event))
    return;

  if (event->type() == ui::ET_SCROLL) {
#if !defined(OS_WIN)
    // TODO(ananta)
    // Investigate if this is true for Windows 8 Metro ASH as well.
    if (event->finger_count() != 2)
      return;
#endif
    blink::WebGestureEvent gesture_event =
        MakeWebGestureEventFlingCancel();
    host_->ForwardGestureEvent(gesture_event);
    blink::WebMouseWheelEvent mouse_wheel_event =
        MakeWebMouseWheelEvent(event);
    host_->ForwardWheelEvent(mouse_wheel_event);
    RecordAction(base::UserMetricsAction("TrackpadScroll"));
  } else if (event->type() == ui::ET_SCROLL_FLING_START ||
             event->type() == ui::ET_SCROLL_FLING_CANCEL) {
    blink::WebGestureEvent gesture_event =
        MakeWebGestureEvent(event);
    host_->ForwardGestureEvent(gesture_event);
    if (event->type() == ui::ET_SCROLL_FLING_START)
      RecordAction(base::UserMetricsAction("TrackpadScrollFling"));
  }

  event->SetHandled();
}

void RenderWidgetHostViewAura::OnTouchEvent(ui::TouchEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewAura::OnTouchEvent");
  if (touch_editing_client_ && touch_editing_client_->HandleInputEvent(event))
    return;

  // Update the touch event first.
  blink::WebTouchPoint* point = UpdateWebTouchEventFromUIEvent(*event,
                                                                &touch_event_);

  // Forward the touch event only if a touch point was updated, and there's a
  // touch-event handler in the page, and no other touch-event is in the queue.
  // It is important to always consume the event if there is a touch-event
  // handler in the page, or some touch-event is already in the queue, even if
  // no point has been updated, to make sure that this event does not get
  // processed by the gesture recognizer before the events in the queue.
  if (host_->ShouldForwardTouchEvent())
    event->StopPropagation();

  if (point) {
    if (host_->ShouldForwardTouchEvent())
      host_->ForwardTouchEventWithLatencyInfo(touch_event_, *event->latency());
    UpdateWebTouchEventAfterDispatch(&touch_event_, point);
  }
}

void RenderWidgetHostViewAura::OnGestureEvent(ui::GestureEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewAura::OnGestureEvent");
  if ((event->type() == ui::ET_GESTURE_PINCH_BEGIN ||
      event->type() == ui::ET_GESTURE_PINCH_UPDATE ||
      event->type() == ui::ET_GESTURE_PINCH_END) && !pinch_zoom_enabled_) {
    event->SetHandled();
    return;
  }

  if (touch_editing_client_ && touch_editing_client_->HandleInputEvent(event))
    return;

  RenderViewHostDelegate* delegate = NULL;
  if (host_->IsRenderView())
    delegate = RenderViewHost::From(host_)->GetDelegate();

  if (delegate && event->type() == ui::ET_GESTURE_BEGIN &&
      event->details().touch_points() == 1) {
    delegate->HandleGestureBegin();
  }

  blink::WebGestureEvent gesture = MakeWebGestureEvent(event);
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    // Webkit does not stop a fling-scroll on tap-down. So explicitly send an
    // event to stop any in-progress flings.
    blink::WebGestureEvent fling_cancel = gesture;
    fling_cancel.type = blink::WebInputEvent::GestureFlingCancel;
    fling_cancel.sourceDevice = blink::WebGestureEvent::Touchscreen;
    host_->ForwardGestureEvent(fling_cancel);
  }

  if (gesture.type != blink::WebInputEvent::Undefined) {
    host_->ForwardGestureEventWithLatencyInfo(gesture, *event->latency());

    if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
        event->type() == ui::ET_GESTURE_SCROLL_UPDATE ||
        event->type() == ui::ET_GESTURE_SCROLL_END) {
      RecordAction(base::UserMetricsAction("TouchscreenScroll"));
    } else if (event->type() == ui::ET_SCROLL_FLING_START) {
      RecordAction(base::UserMetricsAction("TouchscreenScrollFling"));
    }
  }

  if (delegate && event->type() == ui::ET_GESTURE_END &&
      event->details().touch_points() == 1) {
    delegate->HandleGestureEnd();
  }

  // If a gesture is not processed by the webpage, then WebKit processes it
  // (e.g. generates synthetic mouse events).
  event->SetHandled();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::client::ActivationDelegate implementation:

bool RenderWidgetHostViewAura::ShouldActivate() const {
  aura::WindowTreeHost* host = window_->GetHost();
  if (!host)
    return true;
  const ui::Event* event = host->dispatcher()->current_event();
  if (!event)
    return true;
  return is_fullscreen_;
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura,
//     aura::client::ActivationChangeObserver implementation:

void RenderWidgetHostViewAura::OnWindowActivated(aura::Window* gained_active,
                                                 aura::Window* lost_active) {
  DCHECK(window_ == gained_active || window_ == lost_active);
  if (window_ == gained_active) {
    const ui::Event* event = window_->GetHost()->dispatcher()->current_event();
    if (event && PointerEventActivates(*event))
      host_->OnPointerEventActivate();
  }
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::client::CursorClientObserver implementation:

void RenderWidgetHostViewAura::OnCursorVisibilityChanged(bool is_visible) {
  NotifyRendererOfCursorVisibilityState(is_visible);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::client::FocusChangeObserver implementation:

void RenderWidgetHostViewAura::OnWindowFocused(aura::Window* gained_focus,
                                               aura::Window* lost_focus) {
  DCHECK(window_ == gained_focus || window_ == lost_focus);
  if (window_ == gained_focus) {
    // We need to honor input bypass if the associated tab is does not want
    // input. This gives the current focused window a chance to be the text
    // input client and handle events.
    if (host_->ignore_input_events())
      return;

    host_->GotFocus();
    host_->SetActive(true);

    ui::InputMethod* input_method = GetInputMethod();
    if (input_method) {
      // Ask the system-wide IME to send all TextInputClient messages to |this|
      // object.
      input_method->SetFocusedTextInputClient(this);
      host_->SetInputMethodActive(input_method->IsActive());

      // Often the application can set focus to the view in response to a key
      // down. However the following char event shouldn't be sent to the web
      // page.
      host_->SuppressNextCharEvents();
    } else {
      host_->SetInputMethodActive(false);
    }

    BrowserAccessibilityManager* manager = GetBrowserAccessibilityManager();
    if (manager)
      manager->OnWindowFocused();
  } else if (window_ == lost_focus) {
    host_->SetActive(false);
    host_->Blur();

    DetachFromInputMethod();
    host_->SetInputMethodActive(false);

    if (touch_editing_client_)
      touch_editing_client_->EndTouchEditing(false);

    BrowserAccessibilityManager* manager = GetBrowserAccessibilityManager();
    if (manager)
      manager->OnWindowBlurred();

    // If we lose the focus while fullscreen, close the window; Pepper Flash
    // won't do it for us (unlike NPAPI Flash). However, we do not close the
    // window if we lose the focus to a window on another display.
    gfx::Screen* screen = gfx::Screen::GetScreenFor(window_);
    bool focusing_other_display =
        gained_focus && screen->GetNumDisplays() > 1 &&
        (screen->GetDisplayNearestWindow(window_).id() !=
         screen->GetDisplayNearestWindow(gained_focus).id());
    if (is_fullscreen_ && !in_shutdown_ && !focusing_other_display) {
#if defined(OS_WIN)
      // On Windows, if we are switching to a non Aura Window on a different
      // screen we should not close the fullscreen window.
      if (!gained_focus) {
        POINT point = {0};
        ::GetCursorPos(&point);
        if (screen->GetDisplayNearestWindow(window_).id() !=
            screen->GetDisplayNearestPoint(gfx::Point(point)).id())
          return;
      }
#endif
      in_shutdown_ = true;
      host_->Shutdown();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::WindowTreeHostObserver implementation:

void RenderWidgetHostViewAura::OnHostMoved(const aura::WindowTreeHost* host,
                                           const gfx::Point& new_origin) {
  TRACE_EVENT1("ui", "RenderWidgetHostViewAura::OnHostMoved",
               "new_origin", new_origin.ToString());

  UpdateScreenInfo(window_);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::CompositorObserver implementation:

void RenderWidgetHostViewAura::OnCompositingDidCommit(
    ui::Compositor* compositor) {
  if (can_lock_compositor_ == NO_PENDING_COMMIT) {
    can_lock_compositor_ = YES;
    if (resize_lock_.get() && resize_lock_->GrabDeferredLock())
      can_lock_compositor_ = YES_DID_LOCK;
  }
  RunOnCommitCallbacks();
  if (resize_lock_ &&
      resize_lock_->expected_size() == current_frame_size_in_dip_) {
    resize_lock_.reset();
    host_->WasResized();
    // We may have had a resize while we had the lock (e.g. if the lock expired,
    // or if the UI still gave us some resizes), so make sure we grab a new lock
    // if necessary.
    MaybeCreateResizeLock();
  }
}

void RenderWidgetHostViewAura::OnCompositingStarted(
    ui::Compositor* compositor, base::TimeTicks start_time) {
  last_draw_ended_ = start_time;
}

void RenderWidgetHostViewAura::OnCompositingEnded(
    ui::Compositor* compositor) {
}

void RenderWidgetHostViewAura::OnCompositingAborted(
    ui::Compositor* compositor) {
}

void RenderWidgetHostViewAura::OnCompositingLockStateChanged(
    ui::Compositor* compositor) {
  // A compositor lock that is part of a resize lock timed out. We
  // should display a renderer frame.
  if (!compositor->IsLocked() && can_lock_compositor_ == YES_DID_LOCK) {
    can_lock_compositor_ = NO_PENDING_RENDERER_FRAME;
  }
}

void RenderWidgetHostViewAura::OnUpdateVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  if (IsShowing())
    host_->UpdateVSyncParameters(timebase, interval);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, BrowserAccessibilityDelegate implementation:

void RenderWidgetHostViewAura::SetAccessibilityFocus(int acc_obj_id) {
  if (!host_)
    return;

  host_->AccessibilitySetFocus(acc_obj_id);
}

void RenderWidgetHostViewAura::AccessibilityDoDefaultAction(int acc_obj_id) {
  if (!host_)
    return;

  host_->AccessibilityDoDefaultAction(acc_obj_id);
}

void RenderWidgetHostViewAura::AccessibilityScrollToMakeVisible(
    int acc_obj_id, gfx::Rect subfocus) {
  if (!host_)
    return;

  host_->AccessibilityScrollToMakeVisible(acc_obj_id, subfocus);
}

void RenderWidgetHostViewAura::AccessibilityScrollToPoint(
    int acc_obj_id, gfx::Point point) {
  if (!host_)
    return;

  host_->AccessibilityScrollToPoint(acc_obj_id, point);
}

void RenderWidgetHostViewAura::AccessibilitySetTextSelection(
    int acc_obj_id, int start_offset, int end_offset) {
  if (!host_)
    return;

  host_->AccessibilitySetTextSelection(
      acc_obj_id, start_offset, end_offset);
}

gfx::Point RenderWidgetHostViewAura::GetLastTouchEventLocation() const {
  // Only needed for Win 8 non-aura.
  return gfx::Point();
}

void RenderWidgetHostViewAura::FatalAccessibilityTreeError() {
  host_->FatalAccessibilityTreeError();
  SetBrowserAccessibilityManager(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ImageTransportFactoryObserver implementation:

void RenderWidgetHostViewAura::OnLostResources() {
  if (frame_provider_.get())
    EvictDelegatedFrame();
  idle_frame_subscriber_textures_.clear();
  yuv_readback_pipeline_.reset();

  host_->ScheduleComposite();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, private:

RenderWidgetHostViewAura::~RenderWidgetHostViewAura() {
  if (touch_editing_client_)
    touch_editing_client_->OnViewDestroyed();

  ImageTransportFactory::GetInstance()->RemoveObserver(this);

  window_observer_.reset();
  if (window_->GetHost())
    window_->GetHost()->RemoveObserver(this);
  UnlockMouse();
  if (popup_parent_host_view_) {
    DCHECK(popup_parent_host_view_->popup_child_host_view_ == NULL ||
           popup_parent_host_view_->popup_child_host_view_ == this);
    popup_parent_host_view_->popup_child_host_view_ = NULL;
  }
  if (popup_child_host_view_) {
    DCHECK(popup_child_host_view_->popup_parent_host_view_ == NULL ||
           popup_child_host_view_->popup_parent_host_view_ == this);
    popup_child_host_view_->popup_parent_host_view_ = NULL;
  }
  event_filter_for_popup_exit_.reset();
  aura::client::SetTooltipText(window_, NULL);
  gfx::Screen::GetScreenFor(window_)->RemoveObserver(this);

  // This call is usually no-op since |this| object is already removed from the
  // Aura root window and we don't have a way to get an input method object
  // associated with the window, but just in case.
  DetachFromInputMethod();

  if (resource_collection_.get())
    resource_collection_->SetClient(NULL);

  // An OwnedMailbox should not refer to the GLHelper anymore once the RWHVA is
  // destroyed, as it may then outlive the GLHelper.
  for (std::set<OwnedMailbox*>::iterator it =
           active_frame_subscriber_textures_.begin();
       it != active_frame_subscriber_textures_.end();
       ++it) {
    (*it)->Destroy();
  }
  active_frame_subscriber_textures_.clear();

#if defined(OS_WIN)
  legacy_render_widget_host_HWND_.reset(NULL);
#endif

  DCHECK(!vsync_manager_);
}

void RenderWidgetHostViewAura::UpdateCursorIfOverSelf() {
  const gfx::Point screen_point =
      gfx::Screen::GetScreenFor(GetNativeView())->GetCursorScreenPoint();
  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window)
    return;

  gfx::Point root_window_point = screen_point;
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  if (screen_position_client) {
    screen_position_client->ConvertPointFromScreen(
        root_window, &root_window_point);
  }

  if (root_window->GetEventHandlerForPoint(root_window_point) != window_)
    return;

  gfx::NativeCursor cursor = current_cursor_.GetNativeCursor();
  // Do not show loading cursor when the cursor is currently hidden.
  if (is_loading_ && cursor != ui::kCursorNone)
    cursor = ui::kCursorPointer;

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client) {
    cursor_client->SetCursor(cursor);
  }
}

ui::InputMethod* RenderWidgetHostViewAura::GetInputMethod() const {
  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window)
    return NULL;
  return root_window->GetProperty(aura::client::kRootWindowInputMethodKey);
}

bool RenderWidgetHostViewAura::NeedsInputGrab() {
  return popup_type_ == blink::WebPopupTypeSelect;
}

void RenderWidgetHostViewAura::FinishImeCompositionSession() {
  if (!has_composition_text_)
    return;
  if (host_) {
    host_->ImeConfirmComposition(base::string16(), gfx::Range::InvalidRange(),
                                 false);
  }
  ImeCancelComposition();
}

void RenderWidgetHostViewAura::ModifyEventMovementAndCoords(
    blink::WebMouseEvent* event) {
  // If the mouse has just entered, we must report zero movementX/Y. Hence we
  // reset any global_mouse_position set previously.
  if (event->type == blink::WebInputEvent::MouseEnter ||
      event->type == blink::WebInputEvent::MouseLeave)
    global_mouse_position_.SetPoint(event->globalX, event->globalY);

  // Movement is computed by taking the difference of the new cursor position
  // and the previous. Under mouse lock the cursor will be warped back to the
  // center so that we are not limited by clipping boundaries.
  // We do not measure movement as the delta from cursor to center because
  // we may receive more mouse movement events before our warp has taken
  // effect.
  event->movementX = event->globalX - global_mouse_position_.x();
  event->movementY = event->globalY - global_mouse_position_.y();

  global_mouse_position_.SetPoint(event->globalX, event->globalY);

  // Under mouse lock, coordinates of mouse are locked to what they were when
  // mouse lock was entered.
  if (mouse_locked_) {
    event->x = unlocked_mouse_position_.x();
    event->y = unlocked_mouse_position_.y();
    event->windowX = unlocked_mouse_position_.x();
    event->windowY = unlocked_mouse_position_.y();
    event->globalX = unlocked_global_mouse_position_.x();
    event->globalY = unlocked_global_mouse_position_.y();
  } else {
    unlocked_mouse_position_.SetPoint(event->windowX, event->windowY);
    unlocked_global_mouse_position_.SetPoint(event->globalX, event->globalY);
  }
}

void RenderWidgetHostViewAura::NotifyRendererOfCursorVisibilityState(
    bool is_visible) {
  if (host_->is_hidden() ||
      (cursor_visibility_state_in_renderer_ == VISIBLE && is_visible) ||
      (cursor_visibility_state_in_renderer_ == NOT_VISIBLE && !is_visible))
    return;

  cursor_visibility_state_in_renderer_ = is_visible ? VISIBLE : NOT_VISIBLE;
  host_->SendCursorVisibilityState(is_visible);
}

void RenderWidgetHostViewAura::SchedulePaintIfNotInClip(
    const gfx::Rect& rect,
    const gfx::Rect& clip) {
  if (!clip.IsEmpty()) {
    gfx::Rect to_paint = gfx::SubtractRects(rect, clip);
    if (!to_paint.IsEmpty())
      window_->SchedulePaintInRect(to_paint);
  } else {
    window_->SchedulePaintInRect(rect);
  }
}

bool RenderWidgetHostViewAura::ShouldMoveToCenter() {
  gfx::Rect rect = window_->bounds();
  rect = ConvertRectToScreen(rect);
  int border_x = rect.width() * kMouseLockBorderPercentage / 100;
  int border_y = rect.height() * kMouseLockBorderPercentage / 100;

  return global_mouse_position_.x() < rect.x() + border_x ||
      global_mouse_position_.x() > rect.right() - border_x ||
      global_mouse_position_.y() < rect.y() + border_y ||
      global_mouse_position_.y() > rect.bottom() - border_y;
}

void RenderWidgetHostViewAura::RunOnCommitCallbacks() {
  for (std::vector<base::Closure>::const_iterator
      it = on_compositing_did_commit_callbacks_.begin();
      it != on_compositing_did_commit_callbacks_.end(); ++it) {
    it->Run();
  }
  on_compositing_did_commit_callbacks_.clear();
}

void RenderWidgetHostViewAura::AddOnCommitCallbackAndDisableLocks(
    const base::Closure& callback) {
  ui::Compositor* compositor = GetCompositor();
  DCHECK(compositor);

  if (!compositor->HasObserver(this))
    compositor->AddObserver(this);

  can_lock_compositor_ = NO_PENDING_COMMIT;
  on_compositing_did_commit_callbacks_.push_back(callback);
}

void RenderWidgetHostViewAura::AddedToRootWindow() {
  window_->GetHost()->AddObserver(this);
  UpdateScreenInfo(window_);

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());
  if (cursor_client) {
    cursor_client->AddObserver(this);
    NotifyRendererOfCursorVisibilityState(cursor_client->IsCursorVisible());
  }
  if (HasFocus()) {
    ui::InputMethod* input_method = GetInputMethod();
    if (input_method)
      input_method->SetFocusedTextInputClient(this);
  }

#if defined(OS_WIN)
  // The parent may have changed here. Ensure that the legacy window is
  // reparented accordingly.
  if (legacy_render_widget_host_HWND_)
    legacy_render_widget_host_HWND_->UpdateParent(
        reinterpret_cast<HWND>(GetNativeViewId()));
#endif

  ui::Compositor* compositor = GetCompositor();
  if (compositor) {
    DCHECK(!vsync_manager_);
    vsync_manager_ = compositor->vsync_manager();
    vsync_manager_->AddObserver(this);
  }
}

void RenderWidgetHostViewAura::RemovingFromRootWindow() {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());
  if (cursor_client)
    cursor_client->RemoveObserver(this);

  DetachFromInputMethod();

  window_->GetHost()->RemoveObserver(this);
  RunOnCommitCallbacks();
  resize_lock_.reset();
  host_->WasResized();
  ui::Compositor* compositor = GetCompositor();
  if (compositor && compositor->HasObserver(this))
    compositor->RemoveObserver(this);

#if defined(OS_WIN)
  // Update the legacy window's parent temporarily to the desktop window. It
  // will eventually get reparented to the right root.
  if (legacy_render_widget_host_HWND_)
    legacy_render_widget_host_HWND_->UpdateParent(::GetDesktopWindow());
#endif

  if (vsync_manager_) {
    vsync_manager_->RemoveObserver(this);
    vsync_manager_ = NULL;
  }
}

ui::Compositor* RenderWidgetHostViewAura::GetCompositor() const {
  aura::WindowTreeHost* host = window_->GetHost();
  return host ? host->compositor() : NULL;
}

void RenderWidgetHostViewAura::DetachFromInputMethod() {
  ui::InputMethod* input_method = GetInputMethod();
  if (input_method && input_method->GetTextInputClient() == this)
    input_method->SetFocusedTextInputClient(NULL);
}

void RenderWidgetHostViewAura::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  ui::TextEditKeyBindingsDelegateAuraLinux* keybinding_delegate =
      ui::GetTextEditKeyBindingsDelegate();
  std::vector<ui::TextEditCommandAuraLinux> commands;
  if (!event.skip_in_browser &&
      keybinding_delegate &&
      event.os_event &&
      keybinding_delegate->MatchEvent(*event.os_event, &commands)) {
    // Transform from ui/ types to content/ types.
    EditCommands edit_commands;
    for (std::vector<ui::TextEditCommandAuraLinux>::const_iterator it =
             commands.begin(); it != commands.end(); ++it) {
      edit_commands.push_back(EditCommand(it->GetCommandString(),
                                          it->argument()));
    }
    host_->Send(new InputMsg_SetEditCommandsForNextKeyEvent(
        host_->GetRoutingID(), edit_commands));
    NativeWebKeyboardEvent copy_event(event);
    copy_event.match_edit_command = true;
    host_->ForwardKeyboardEvent(copy_event);
    return;
  }
#endif

  host_->ForwardKeyboardEvent(event);
}

void RenderWidgetHostViewAura::LockResources() {
  DCHECK(frame_provider_);
  delegated_frame_evictor_->LockFrame();
}

void RenderWidgetHostViewAura::UnlockResources() {
  DCHECK(frame_provider_);
  delegated_frame_evictor_->UnlockFrame();
}

SkBitmap::Config RenderWidgetHostViewAura::PreferredReadbackFormat() {
  return SkBitmap::kARGB_8888_Config;
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::LayerOwnerDelegate implementation:

void RenderWidgetHostViewAura::OnLayerRecreated(ui::Layer* old_layer,
                                                ui::Layer* new_layer) {
  // The new_layer is the one that will be used by our Window, so that's the one
  // that should keep our frame. old_layer will be returned to the
  // RecreateLayer caller, and should have a copy.
  if (frame_provider_.get()) {
    new_layer->SetShowDelegatedContent(frame_provider_.get(),
                                       current_frame_size_in_dip_);
  }
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostView, public:

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewAura(widget);
}

// static
void RenderWidgetHostViewPort::GetDefaultScreenInfo(WebScreenInfo* results) {
  GetScreenInfoForWindow(results, NULL);
}

}  // namespace content
