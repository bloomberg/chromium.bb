// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_win.h"

#include <algorithm>
#include <map>
#include <peninputpanel_i.c>
#include <stack>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/process_util.h"
#include "base/threading/thread.h"
#include "base/win/metro.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "base/win/wrapped_window_proc.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/renderer_host/backing_store.h"
#include "content/browser/renderer_host/backing_store_win.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/accessibility_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/plugin_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/process_type.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebInputEventFactory.h"
#include "ui/base/events/event.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/text/text_elider.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/view_prop.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/base/win/mouse_wheel_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "webkit/glue/webcursor.h"
#include "webkit/plugins/npapi/plugin_constants_win.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"

using base::TimeDelta;
using base::TimeTicks;
using ui::ViewProp;
using WebKit::WebInputEvent;
using WebKit::WebInputEventFactory;
using WebKit::WebMouseEvent;
using WebKit::WebTextDirection;

namespace content {
namespace {

// Tooltips will wrap after this width. Yes, wrap. Imagine that!
const int kTooltipMaxWidthPixels = 300;

// Maximum number of characters we allow in a tooltip.
const int kMaxTooltipLength = 1024;

// A custom MSAA object id used to determine if a screen reader is actively
// listening for MSAA events.
const int kIdCustom = 1;

// The delay before the compositor host window is destroyed. This gives the GPU
// process a grace period to stop referencing it.
const int kDestroyCompositorHostWindowDelay = 10000;

// In mouse lock mode, we need to prevent the (invisible) cursor from hitting
// the border of the view, in order to get valid movement information. However,
// forcing the cursor back to the center of the view after each mouse move
// doesn't work well. It reduces the frequency of useful WM_MOUSEMOVE messages
// significantly. Therefore, we move the cursor to the center of the view only
// if it approaches the border. |kMouseLockBorderPercentage| specifies the width
// of the border area, in percentage of the corresponding dimension.
const int kMouseLockBorderPercentage = 15;

// We display the onscreen keyboard in the context of a WM_POINTERDOWN message.
// This context is reset in a delayed task. Theory being that the page should
// have enough time to change focus before this context is reset.
// TODO(ananta)
// Refine this.
const int kPointerDownContextResetDelay = 200;

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

void SendToGpuProcessHost(int gpu_host_id, scoped_ptr<IPC::Message> message) {
  GpuProcessHost* gpu_process_host = GpuProcessHost::FromID(gpu_host_id);
  if (!gpu_process_host)
    return;

  gpu_process_host->Send(message.release());
}

void PostTaskOnIOThread(const tracked_objects::Location& from_here,
                        base::Closure task) {
  BrowserThread::PostTask(BrowserThread::IO, from_here, task);
}

bool DecodeZoomGesture(HWND hwnd, const GESTUREINFO& gi,
                       PageZoom* zoom, POINT* zoom_center) {
  static long start = 0;
  static POINT zoom_first;

  if (gi.dwFlags == GF_BEGIN) {
    start = gi.ullArguments;
    zoom_first.x = gi.ptsLocation.x;
    zoom_first.y = gi.ptsLocation.y;
    ScreenToClient(hwnd, &zoom_first);
    return false;
  }

  if (gi.dwFlags == GF_END)
    return false;

  POINT zoom_second = {0};
  zoom_second.x = gi.ptsLocation.x;
  zoom_second.y = gi.ptsLocation.y;
  ScreenToClient(hwnd, &zoom_second);

  if (zoom_first.x == zoom_second.x && zoom_first.y == zoom_second.y)
    return false;

  zoom_center->x = (zoom_first.x + zoom_second.x) / 2;
  zoom_center->y = (zoom_first.y + zoom_second.y) / 2;

  double zoom_factor =
      static_cast<double>(gi.ullArguments)/static_cast<double>(start);

  *zoom = zoom_factor >= 1 ? PAGE_ZOOM_IN : PAGE_ZOOM_OUT;

  start = gi.ullArguments;
  zoom_first = zoom_second;
  return true;
}

bool DecodeScrollGesture(const GESTUREINFO& gi,
                         POINT* start,
                         POINT* delta){
  // Windows gestures are streams of messages with begin/end messages that
  // separate each new gesture. We key off the begin message to reset
  // the static variables.
  static POINT last_pt;
  static POINT start_pt;

  if (gi.dwFlags == GF_BEGIN) {
    delta->x = 0;
    delta->y = 0;
    start_pt.x = gi.ptsLocation.x;
    start_pt.y = gi.ptsLocation.y;
  } else {
    delta->x = gi.ptsLocation.x - last_pt.x;
    delta->y = gi.ptsLocation.y - last_pt.y;
  }
  last_pt.x = gi.ptsLocation.x;
  last_pt.y = gi.ptsLocation.y;
  *start = start_pt;
  return true;
}

WebKit::WebMouseWheelEvent MakeFakeScrollWheelEvent(HWND hwnd,
                                                    POINT start,
                                                    POINT delta) {
  WebKit::WebMouseWheelEvent result;
  result.type = WebInputEvent::MouseWheel;
  result.timeStampSeconds = ::GetMessageTime() / 1000.0;
  result.button = WebMouseEvent::ButtonNone;
  result.globalX = start.x;
  result.globalY = start.y;
  // Map to window coordinates.
  POINT client_point = { result.globalX, result.globalY };
  MapWindowPoints(0, hwnd, &client_point, 1);
  result.x = client_point.x;
  result.y = client_point.y;
  result.windowX = result.x;
  result.windowY = result.y;
  // Note that we support diagonal scrolling.
  result.deltaX = static_cast<float>(delta.x);
  result.wheelTicksX = WHEEL_DELTA;
  result.deltaY = static_cast<float>(delta.y);
  result.wheelTicksY = WHEEL_DELTA;
  return result;
}

static const int kTouchMask = 0x7;

inline int GetTouchType(const TOUCHINPUT& point) {
  return point.dwFlags & kTouchMask;
}

inline void SetTouchType(TOUCHINPUT* point, int type) {
  point->dwFlags = (point->dwFlags & kTouchMask) | type;
}

ui::EventType ConvertToUIEvent(WebKit::WebTouchPoint::State t) {
  switch (t) {
    case WebKit::WebTouchPoint::StatePressed:
      return ui::ET_TOUCH_PRESSED;
    case WebKit::WebTouchPoint::StateMoved:
      return ui::ET_TOUCH_MOVED;
    case WebKit::WebTouchPoint::StateStationary:
      return ui::ET_TOUCH_STATIONARY;
    case WebKit::WebTouchPoint::StateReleased:
      return ui::ET_TOUCH_RELEASED;
    case WebKit::WebTouchPoint::StateCancelled:
      return ui::ET_TOUCH_CANCELLED;
    default:
      DCHECK(false) << "Unexpected ui type. " << t;
      return ui::ET_UNKNOWN;
  }
}

WebKit::WebInputEvent::Type ConvertToWebInputEvent(ui::EventType t) {
  switch (t) {
    case ui::ET_UNKNOWN:
      return WebKit::WebInputEvent::Undefined;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      return WebKit::WebGestureEvent::GestureScrollBegin;
    case ui::ET_GESTURE_SCROLL_END:
      return WebKit::WebGestureEvent::GestureScrollEnd;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      return WebKit::WebGestureEvent::GestureScrollUpdate;
    case ui::ET_SCROLL_FLING_START:
      // TODO: Confirm that ui and webkit agree on fling api.
      return WebKit::WebGestureEvent::GestureFlingStart;
    case ui::ET_SCROLL_FLING_CANCEL:
      // TODO: Confirm that ui and webkit agree on fling api.
      return WebKit::WebGestureEvent::GestureFlingCancel;
    case ui::ET_GESTURE_TAP:
      return WebKit::WebGestureEvent::GestureTap;
    case ui::ET_GESTURE_TAP_DOWN:
      return WebKit::WebGestureEvent::GestureTapDown;
    case ui::ET_GESTURE_TAP_CANCEL:
      return WebKit::WebGestureEvent::GestureTapCancel;
    case ui::ET_GESTURE_DOUBLE_TAP:
      return WebKit::WebGestureEvent::GestureDoubleTap;
    case ui::ET_GESTURE_LONG_PRESS:
      return WebKit::WebGestureEvent::GestureLongPress;
    case ui::ET_GESTURE_PINCH_BEGIN:
      return WebKit::WebGestureEvent::GesturePinchBegin;
    case ui::ET_GESTURE_PINCH_END:
      return WebKit::WebGestureEvent::GesturePinchEnd;
    case ui::ET_GESTURE_PINCH_UPDATE:
      return WebKit::WebGestureEvent::GesturePinchUpdate;
    case ui::ET_GESTURE_BEGIN:
      return WebKit::WebGestureEvent::Undefined;
    case ui::ET_GESTURE_END:
      return WebKit::WebGestureEvent::Undefined;
    case ui::ET_GESTURE_MULTIFINGER_SWIPE:
      return WebKit::WebGestureEvent::Undefined;
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      return WebKit::WebGestureEvent::GestureTwoFingerTap;
    case ui::ET_TOUCH_PRESSED:
      return WebKit::WebInputEvent::TouchStart;
    case ui::ET_TOUCH_MOVED:
    case ui::ET_TOUCH_STATIONARY:
      return WebKit::WebInputEvent::TouchMove;
    case ui::ET_TOUCH_RELEASED:
      return WebKit::WebInputEvent::TouchEnd;
    case ui::ET_TOUCH_CANCELLED:
      return WebKit::WebInputEvent::TouchCancel;
    default:
      DCHECK(false) << "Unexpected ui type. " << t;
      return WebKit::WebInputEvent::Undefined;
  }
}

WebKit::WebGestureEvent CreateWebGestureEvent(HWND hwnd,
                                              const ui::GestureEvent& gesture) {
  POINT client_point = gesture.location().ToPOINT();
  POINT screen_point = gesture.location().ToPOINT();
  MapWindowPoints(::GetParent(hwnd), hwnd, &client_point, 1);
  MapWindowPoints(hwnd, HWND_DESKTOP, &screen_point, 1);

  WebKit::WebGestureEvent gesture_event;
  gesture_event.type = ConvertToWebInputEvent(gesture.type());
  gesture_event.x = client_point.x;
  gesture_event.y = client_point.y;
  gesture_event.globalX = screen_point.x;
  gesture_event.globalY = screen_point.y;
  gesture_event.modifiers =
      (base::win::IsShiftPressed() ? WebKit::WebGestureEvent::ShiftKey : 0)
      | (base::win::IsCtrlPressed() ? WebKit::WebGestureEvent::ControlKey : 0)
      | (base::win::IsAltPressed() ? WebKit::WebGestureEvent::AltKey : 0);

  // Copy any event-type specific data.
  switch (gesture.type()) {
    case ui::ET_GESTURE_TAP:
      gesture_event.data.tap.tapCount = gesture.details().tap_count();
      gesture_event.data.tap.width =
          gesture.details().bounding_box().width();
      gesture_event.data.tap.height =
          gesture.details().bounding_box().height();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      gesture_event.data.scrollUpdate.deltaX = gesture.details().scroll_x();
      gesture_event.data.scrollUpdate.deltaY = gesture.details().scroll_y();
      break;
    case ui::ET_GESTURE_PINCH_UPDATE:
      gesture_event.data.pinchUpdate.scale = gesture.details().scale();
      break;
    case ui::ET_SCROLL_FLING_START:
      gesture_event.data.flingStart.velocityX = gesture.details().velocity_x();
      gesture_event.data.flingStart.velocityY = gesture.details().velocity_y();
      gesture_event.data.flingStart.sourceDevice =
          WebKit::WebGestureEvent::Touchscreen;
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      gesture_event.data.longPress.width =
          gesture.details().bounding_box().width();
      gesture_event.data.longPress.height =
          gesture.details().bounding_box().height();
      break;
    default:
      break;
  }
  return gesture_event;
}

class TouchEventFromWebTouchPoint : public ui::TouchEvent {
 public:
  TouchEventFromWebTouchPoint(const WebKit::WebTouchPoint& touch_point,
                              base::TimeDelta& timestamp)
      : ui::TouchEvent(ConvertToUIEvent(touch_point.state),
                       touch_point.position,
                       touch_point.id,
                       timestamp) {
    set_radius(touch_point.radiusX, touch_point.radiusY);
    set_rotation_angle(touch_point.rotationAngle);
    set_force(touch_point.force);
  }

  virtual ~TouchEventFromWebTouchPoint() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchEventFromWebTouchPoint);
};

}  // namespace

const wchar_t kRenderWidgetHostHWNDClass[] = L"Chrome_RenderWidgetHostHWND";

// Wrapper for maintaining touchstate associated with a WebTouchEvent.
class WebTouchState {
 public:
  explicit WebTouchState(const RenderWidgetHostViewWin* window);

  // Updates the current touchpoint state with the supplied touches.
  // Touches will be consumed only if they are of the same type (e.g. down,
  // up, move). Returns the number of consumed touches.
  size_t UpdateTouchPoints(TOUCHINPUT* points, size_t count);

  // Marks all active touchpoints as released.
  bool ReleaseTouchPoints();

  // The contained WebTouchEvent.
  const WebKit::WebTouchEvent& touch_event() { return touch_event_; }

  // Returns if any touches are modified in the event.
  bool is_changed() { return touch_event_.changedTouchesLength != 0; }

  void QueueEvents(ui::GestureConsumer* consumer, ui::GestureRecognizer* gr) {
    if (touch_event_.touchesLength > 0)
      touch_count_.push(touch_event_.touchesLength);
    base::TimeDelta timestamp = base::TimeDelta::FromMilliseconds(
        touch_event_.timeStampSeconds * 1000);
    for (size_t i = 0; i < touch_event_.touchesLength; ++i) {
      gr->QueueTouchEventForGesture(consumer,
          TouchEventFromWebTouchPoint(touch_event_.touches[i], timestamp));
    }
  }

  int GetNextTouchCount() {
    DCHECK(!touch_count_.empty());
    int result = touch_count_.top();
    touch_count_.pop();
    return result;
  }

 private:
  typedef std::map<unsigned int, int> MapType;

  // Adds a touch point or returns NULL if there's not enough space.
  WebKit::WebTouchPoint* AddTouchPoint(TOUCHINPUT* touch_input);

  // Copy details from a TOUCHINPUT to an existing WebTouchPoint, returning
  // true if the resulting point is a stationary move.
  bool UpdateTouchPoint(WebKit::WebTouchPoint* touch_point,
                        TOUCHINPUT* touch_input);

  // Find (or create) a mapping for _os_touch_id_.
  unsigned int GetMappedTouch(unsigned int os_touch_id);

  // Remove any mappings that are no longer in use.
  void RemoveExpiredMappings();

  // The gesture recognizer processes touch events one at a time, but WebKit
  // (ForwardTouchEvent) takes a set of touch events. |touchCount_| tracks how
  // many individual touch events were sent to ForwardTouchEvent, so we can
  // send the correct number of AdvanceTouchQueue's
  std::stack<int> touch_count_;

  WebKit::WebTouchEvent touch_event_;
  const RenderWidgetHostViewWin* const window_;

  // Maps OS touch Id's into an internal (WebKit-friendly) touch-id.
  // WebKit expects small consecutive integers, starting at 0.
  MapType touch_map_;

  DISALLOW_COPY_AND_ASSIGN(WebTouchState);
};

typedef void (*MetroSetFrameWindow)(HWND window);
typedef void (*MetroCloseFrameWindow)(HWND window);

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewWin, public:

RenderWidgetHostViewWin::RenderWidgetHostViewWin(RenderWidgetHost* widget)
    : render_widget_host_(RenderWidgetHostImpl::From(widget)),
      compositor_host_window_(NULL),
      hide_compositor_window_at_next_paint_(false),
      track_mouse_leave_(false),
      ime_notification_(false),
      capture_enter_key_(false),
      is_hidden_(false),
      about_to_validate_and_paint_(false),
      close_on_deactivate_(false),
      being_destroyed_(false),
      tooltip_hwnd_(NULL),
      tooltip_showing_(false),
      weak_factory_(this),
      is_loading_(false),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      is_fullscreen_(false),
      ignore_mouse_movement_(true),
      composition_range_(ui::Range::InvalidRange()),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          touch_state_(new WebTouchState(this))),
      pointer_down_context_(false),
      focus_on_editable_field_(false),
      received_focus_change_after_pointer_down_(false),
      touch_events_enabled_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          gesture_recognizer_(ui::GestureRecognizer::Create(this))) {
  render_widget_host_->SetView(this);
  registrar_.Add(this,
                 NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                 NotificationService::AllBrowserContextsAndSources());
}

RenderWidgetHostViewWin::~RenderWidgetHostViewWin() {
  UnlockMouse();
  ResetTooltip();
}

void RenderWidgetHostViewWin::CreateWnd(HWND parent) {
  // ATL function to create the window.
  Create(parent);
  // Creating an instance of the text input panel is crashy. Will reenable this
  // after investigation.
  // TODO(ananta)
#if 0
  if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
      !base::win::IsMetroProcess()) {
    virtual_keyboard_.CreateInstance(CLSID_TextInputPanel, NULL, CLSCTX_INPROC);
    if (virtual_keyboard_) {
      virtual_keyboard_->put_AttachedEditWindow(m_hWnd);
      virtual_keyboard_->SetInPlaceVisibility(FALSE);
    } else {
      NOTREACHED() << "Failed to create instance of pen input panel";
    }
  }
#endif
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewWin, RenderWidgetHostView implementation:

void RenderWidgetHostViewWin::InitAsChild(
    gfx::NativeView parent_view) {
  CreateWnd(parent_view);
}

void RenderWidgetHostViewWin::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  close_on_deactivate_ = true;
  DoPopupOrFullscreenInit(parent_host_view->GetNativeView(), pos,
                          WS_EX_TOOLWINDOW);
}

void RenderWidgetHostViewWin::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  gfx::Rect pos = gfx::Screen::GetDisplayNearestWindow(
      reference_host_view->GetNativeView()).bounds();
  is_fullscreen_ = true;
  DoPopupOrFullscreenInit(ui::GetWindowToParentTo(true), pos, 0);
}

RenderWidgetHost* RenderWidgetHostViewWin::GetRenderWidgetHost() const {
  return render_widget_host_;
}

void RenderWidgetHostViewWin::WasShown() {
  if (!is_hidden_)
    return;

  if (web_contents_switch_paint_time_.is_null())
    web_contents_switch_paint_time_ = TimeTicks::Now();
  is_hidden_ = false;

  // |render_widget_host_| may be NULL if the WebContentsImpl is in the process
  // of closing.
  if (render_widget_host_)
    render_widget_host_->WasShown();
}

void RenderWidgetHostViewWin::WasHidden() {
  if (is_hidden_)
    return;

  // If we receive any more paint messages while we are hidden, we want to
  // ignore them so we don't re-allocate the backing store.  We will paint
  // everything again when we become selected again.
  is_hidden_ = true;

  ResetTooltip();

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  if (render_widget_host_)
    render_widget_host_->WasHidden();

  if (accelerated_surface_.get())
    accelerated_surface_->WasHidden();

  if (GetBrowserAccessibilityManager())
    GetBrowserAccessibilityManager()->WasHidden();
}

void RenderWidgetHostViewWin::SetSize(const gfx::Size& size) {
  SetBounds(gfx::Rect(GetViewBounds().origin(), size));
}

void RenderWidgetHostViewWin::SetBounds(const gfx::Rect& rect) {
  if (is_hidden_)
    return;

  // No SWP_NOREDRAW as autofill popups can move and the underneath window
  // should redraw in that case.
  UINT swp_flags = SWP_NOSENDCHANGING | SWP_NOOWNERZORDER | SWP_NOCOPYBITS |
      SWP_NOZORDER | SWP_NOACTIVATE | SWP_DEFERERASE;

  // If the style is not popup, you have to convert the point to client
  // coordinate.
  POINT point = { rect.x(), rect.y() };
  if (GetStyle() & WS_CHILD)
    ScreenToClient(&point);

  SetWindowPos(NULL, point.x, point.y, rect.width(), rect.height(), swp_flags);
  render_widget_host_->WasResized();
}

gfx::NativeView RenderWidgetHostViewWin::GetNativeView() const {
  return m_hWnd;
}

gfx::NativeViewId RenderWidgetHostViewWin::GetNativeViewId() const {
  return reinterpret_cast<gfx::NativeViewId>(m_hWnd);
}

gfx::NativeViewAccessible
RenderWidgetHostViewWin::GetNativeViewAccessible() {
  if (render_widget_host_ &&
      !BrowserAccessibilityState::GetInstance()->IsAccessibleBrowser()) {
    // Attempt to detect screen readers by sending an event with our custom id.
    NotifyWinEvent(EVENT_SYSTEM_ALERT, m_hWnd, kIdCustom, CHILDID_SELF);
  }

  if (!GetBrowserAccessibilityManager()) {
    // Return busy document tree while renderer accessibility tree loads.
    AccessibilityNodeData::State busy_state =
        static_cast<AccessibilityNodeData::State>(
            1 << AccessibilityNodeData::STATE_BUSY);
    SetBrowserAccessibilityManager(
        BrowserAccessibilityManager::CreateEmptyDocument(
            m_hWnd, busy_state, this));
  }

  return GetBrowserAccessibilityManager()->GetRoot()->
      ToBrowserAccessibilityWin();
}

void RenderWidgetHostViewWin::MovePluginWindows(
    const gfx::Point& scroll_offset,
    const std::vector<webkit::npapi::WebPluginGeometry>& plugin_window_moves) {
  MovePluginWindowsHelper(m_hWnd, plugin_window_moves);
}

static BOOL CALLBACK AddChildWindowToVector(HWND hwnd, LPARAM lparam) {
  std::vector<HWND>* vector = reinterpret_cast<std::vector<HWND>*>(lparam);
  vector->push_back(hwnd);
  return TRUE;
}

void RenderWidgetHostViewWin::CleanupCompositorWindow() {
  if (!compositor_host_window_)
    return;

  // Hide the compositor and parent it to the desktop rather than destroying
  // it immediately. The GPU process has a grace period to stop accessing the
  // window. TODO(apatrick): the GPU process should acknowledge that it has
  // finished with the window handle and the browser process should destroy it
  // at that point.
  ::ShowWindow(compositor_host_window_, SW_HIDE);
  ::SetParent(compositor_host_window_, NULL);

  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&::DestroyWindow),
                 compositor_host_window_),
      base::TimeDelta::FromMilliseconds(kDestroyCompositorHostWindowDelay));

  compositor_host_window_ = NULL;
}

bool RenderWidgetHostViewWin::IsActivatable() const {
  // Popups should not be activated.
  return popup_type_ == WebKit::WebPopupTypeNone;
}

void RenderWidgetHostViewWin::Focus() {
  if (IsWindow())
    SetFocus();
}

void RenderWidgetHostViewWin::Blur() {
  NOTREACHED();
}

bool RenderWidgetHostViewWin::HasFocus() const {
  return ::GetFocus() == m_hWnd;
}

bool RenderWidgetHostViewWin::IsSurfaceAvailableForCopy() const {
  return !!render_widget_host_->GetBackingStore(false) ||
      !!accelerated_surface_.get();
}

void RenderWidgetHostViewWin::Show() {
  ShowWindow(SW_SHOW);
  WasShown();
}

void RenderWidgetHostViewWin::Hide() {
  if (!is_fullscreen_ && GetParent() == ui::GetWindowToParentTo(true)) {
    LOG(WARNING) << "Hide() called twice in a row: " << this << ":"
        << GetParent();
    return;
  }

  if (::GetFocus() == m_hWnd)
    ::SetFocus(NULL);
  ShowWindow(SW_HIDE);

  WasHidden();
}

bool RenderWidgetHostViewWin::IsShowing() {
  return !!IsWindowVisible();
}

gfx::Rect RenderWidgetHostViewWin::GetViewBounds() const {
  CRect window_rect;
  GetWindowRect(&window_rect);
  return gfx::Rect(window_rect);
}

void RenderWidgetHostViewWin::UpdateCursor(const WebCursor& cursor) {
  current_cursor_ = cursor;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewWin::UpdateCursorIfOverSelf() {
  static HCURSOR kCursorArrow = LoadCursor(NULL, IDC_ARROW);
  static HCURSOR kCursorAppStarting = LoadCursor(NULL, IDC_APPSTARTING);
  static HINSTANCE module_handle = GetModuleHandle(
      GetContentClient()->browser()->GetResourceDllName());

  // If the mouse is over our HWND, then update the cursor state immediately.
  CPoint pt;
  GetCursorPos(&pt);
  if (WindowFromPoint(pt) == m_hWnd) {
    // We cannot pass in NULL as the module handle as this would only work for
    // standard win32 cursors. We can also receive cursor types which are
    // defined as webkit resources. We need to specify the module handle of
    // chrome.dll while loading these cursors.
    HCURSOR display_cursor = current_cursor_.GetCursor(module_handle);

    // If a page is in the loading state, we want to show the Arrow+Hourglass
    // cursor only when the current cursor is the ARROW cursor. In all other
    // cases we should continue to display the current cursor.
    if (is_loading_ && display_cursor == kCursorArrow)
      display_cursor = kCursorAppStarting;

    SetCursor(display_cursor);
  }
}

void RenderWidgetHostViewWin::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewWin::TextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  // TODO(kinaba): currently, can_compose_inline is ignored and always treated
  // as true. We need to support "can_compose_inline=false" for PPAPI plugins
  // that may want to avoid drawing composition-text by themselves and pass
  // the responsibility to the browser.
  if (text_input_type_ != params.type) {
    text_input_type_ = params.type;
    UpdateIMEState();
  }
}

void RenderWidgetHostViewWin::SelectionBoundsChanged(
    const gfx::Rect& start_rect,
    WebKit::WebTextDirection start_direction,
    const gfx::Rect& end_rect,
    WebKit::WebTextDirection end_direction) {
  bool is_enabled = (text_input_type_ != ui::TEXT_INPUT_TYPE_NONE &&
      text_input_type_ != ui::TEXT_INPUT_TYPE_PASSWORD);
  // Only update caret position if the input method is enabled.
  if (is_enabled) {
    caret_rect_ = start_rect.Union(end_rect);
    ime_input_.UpdateCaretRect(m_hWnd, caret_rect_);
  }
}

void RenderWidgetHostViewWin::ImeCancelComposition() {
  ime_input_.CancelIME(m_hWnd);
}

void RenderWidgetHostViewWin::ImeCompositionRangeChanged(
    const ui::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  composition_range_ = range;
  composition_character_bounds_ = character_bounds;
}

void RenderWidgetHostViewWin::Redraw() {
  RECT damage_bounds;
  GetUpdateRect(&damage_bounds, FALSE);

  base::win::ScopedGDIObject<HRGN> damage_region(CreateRectRgn(0, 0, 0, 0));
  GetUpdateRgn(damage_region, FALSE);

  // Paint the invalid region synchronously.  Our caller will not paint again
  // until we return, so by painting to the screen here, we ensure effective
  // rate-limiting of backing store updates.  This helps a lot on pages that
  // have animations or fairly expensive layout (e.g., google maps).
  //
  // We paint this window synchronously, however child windows (i.e. plugins)
  // are painted asynchronously.  By avoiding synchronous cross-process window
  // message dispatching we allow scrolling to be smooth, and also avoid the
  // browser process locking up if the plugin process is hung.
  //
  RedrawWindow(NULL, damage_region, RDW_UPDATENOW | RDW_NOCHILDREN);

  // Send the invalid rect in screen coordinates.
  gfx::Rect screen_rect = GetViewBounds();
  gfx::Rect invalid_screen_rect(damage_bounds);
  invalid_screen_rect.Offset(screen_rect.x(), screen_rect.y());

  PaintPluginWindowsHelper(m_hWnd, invalid_screen_rect);
}

void RenderWidgetHostViewWin::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
    const std::vector<gfx::Rect>& copy_rects) {
  if (is_hidden_)
    return;

  // Schedule invalidations first so that the ScrollWindowEx call is closer to
  // Redraw.  That minimizes chances of "flicker" resulting if the screen
  // refreshes before we have a chance to paint the exposed area.  Somewhat
  // surprisingly, this ordering matters.

  for (size_t i = 0; i < copy_rects.size(); ++i)
    InvalidateRect(&copy_rects[i].ToRECT(), false);

  if (!scroll_rect.IsEmpty()) {
    RECT clip_rect = scroll_rect.ToRECT();
    ScrollWindowEx(scroll_dx, scroll_dy, NULL, &clip_rect, NULL, NULL,
                   SW_INVALIDATE);
  }

  if (!about_to_validate_and_paint_)
    Redraw();
}

void RenderWidgetHostViewWin::RenderViewGone(base::TerminationStatus status,
                                             int error_code) {
  UpdateCursorIfOverSelf();
  Destroy();
}

void RenderWidgetHostViewWin::WillWmDestroy() {
  CleanupCompositorWindow();
}

void RenderWidgetHostViewWin::Destroy() {
  // We've been told to destroy.
  // By clearing close_on_deactivate_, we prevent further deactivations
  // (caused by windows messages resulting from the DestroyWindow) from
  // triggering further destructions.  The deletion of this is handled by
  // OnFinalMessage();
  close_on_deactivate_ = false;
  render_widget_host_ = NULL;
  being_destroyed_ = true;
  CleanupCompositorWindow();

  if (is_fullscreen_ && base::win::IsMetroProcess()) {
    MetroCloseFrameWindow close_frame_window =
        reinterpret_cast<MetroCloseFrameWindow>(
            ::GetProcAddress(base::win::GetMetroModule(), "CloseFrameWindow"));
    DCHECK(close_frame_window);
    close_frame_window(m_hWnd);
  }

  DestroyWindow();
}

void RenderWidgetHostViewWin::SetTooltipText(const string16& tooltip_text) {
  if (!is_hidden_)
    EnsureTooltip();

  // Clamp the tooltip length to kMaxTooltipLength so that we don't
  // accidentally DOS the user with a mega tooltip (since Windows doesn't seem
  // to do this itself).
  const string16 new_tooltip_text =
      ui::TruncateString(tooltip_text, kMaxTooltipLength);

  if (new_tooltip_text != tooltip_text_) {
    tooltip_text_ = new_tooltip_text;

    // Need to check if the tooltip is already showing so that we don't
    // immediately show the tooltip with no delay when we move the mouse from
    // a region with no tooltip to a region with a tooltip.
    if (::IsWindow(tooltip_hwnd_) && tooltip_showing_) {
      ::SendMessage(tooltip_hwnd_, TTM_POP, 0, 0);
      ::SendMessage(tooltip_hwnd_, TTM_POPUP, 0, 0);
    }
  } else {
    // Make sure the tooltip gets closed after TTN_POP gets sent. For some
    // reason this doesn't happen automatically, so moving the mouse around
    // within the same link/image/etc doesn't cause the tooltip to re-appear.
    if (!tooltip_showing_) {
      if (::IsWindow(tooltip_hwnd_))
        ::SendMessage(tooltip_hwnd_, TTM_POP, 0, 0);
    }
  }
}

BackingStore* RenderWidgetHostViewWin::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStoreWin(render_widget_host_, size);
}

void RenderWidgetHostViewWin::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool)>& callback,
    skia::PlatformCanvas* output) {
  base::ScopedClosureRunner scoped_callback_runner(base::Bind(callback, false));
  if (!accelerated_surface_.get())
    return;

  if (dst_size.IsEmpty())
    return;

  if (!output->initialize(dst_size.width(), dst_size.height(), true))
    return;

  const bool result = accelerated_surface_->CopyTo(
      src_subrect,
      dst_size,
      output->getTopDevice()->accessBitmap(true).getPixels());
  scoped_callback_runner.Release();
  callback.Run(result);
}

void RenderWidgetHostViewWin::SetBackground(const SkBitmap& background) {
  RenderWidgetHostViewBase::SetBackground(background);
  render_widget_host_->SetBackground(background);
}

void RenderWidgetHostViewWin::ProcessTouchAck(
    WebKit::WebInputEvent::Type type, bool processed) {

  DCHECK(render_widget_host_->has_touch_handler() &&
      touch_events_enabled_);

  int touch_count = touch_state_->GetNextTouchCount();
  for (int i = 0; i < touch_count; ++i) {
    scoped_ptr<ui::GestureRecognizer::Gestures> gestures;
    gestures.reset(gesture_recognizer_->AdvanceTouchQueue(this, processed));
    ProcessGestures(gestures.get());
  }

  if (type == WebKit::WebInputEvent::TouchStart)
    UpdateDesiredTouchMode(processed);
}

void RenderWidgetHostViewWin::SetToGestureMode() {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;
  UnregisterTouchWindow(m_hWnd);
  // Single finger panning is consistent with other windows applications.
  const DWORD gesture_allow = GC_PAN_WITH_SINGLE_FINGER_VERTICALLY |
                              GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY;
  const DWORD gesture_block = GC_PAN_WITH_GUTTER;
  GESTURECONFIG gc[] = {
      { GID_ZOOM, GC_ZOOM, 0 },
      { GID_PAN, gesture_allow , gesture_block},
      { GID_TWOFINGERTAP, GC_TWOFINGERTAP , 0},
      { GID_PRESSANDTAP, GC_PRESSANDTAP , 0}
  };
  if (!SetGestureConfig(m_hWnd, 0, arraysize(gc), gc,
      sizeof(GESTURECONFIG))) {
    NOTREACHED();
  }
  touch_events_enabled_ = false;
}

bool RenderWidgetHostViewWin::SetToTouchMode() {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;
  bool touch_mode = RegisterTouchWindow(m_hWnd, TWF_WANTPALM) == TRUE;
  touch_events_enabled_ = touch_mode;
  return touch_mode;
}

void RenderWidgetHostViewWin::UpdateDesiredTouchMode(bool touch_mode) {
  // Make sure that touch events even make sense.
  bool touch_mode_valid = base::win::GetVersion() >= base::win::VERSION_WIN7 &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableTouchEvents);
  touch_mode = touch_mode_valid;

  // Already in correct mode, nothing to do.
  if ((touch_mode && touch_events_enabled_) ||
      (!touch_mode && !touch_events_enabled_))
    return;

  // Now we know that the window's current state doesn't match the desired
  // state. If we want touch mode, then we attempt to register for touch
  // events, and otherwise to unregister.
  if (touch_mode) {
    touch_mode = SetToTouchMode();
  }
  if (!touch_mode) {
    SetToGestureMode();
  }
}

bool RenderWidgetHostViewWin::DispatchLongPressGestureEvent(
    ui::GestureEvent* event) {
  return ForwardGestureEventToRenderer(event);
}

bool RenderWidgetHostViewWin::DispatchCancelTouchEvent(
    ui::TouchEvent* event) {
  if (!render_widget_host_ || !touch_events_enabled_)
    return false;
  DCHECK(event->type() == WebKit::WebInputEvent::TouchCancel);
  WebKit::WebTouchEvent cancel_event;
  cancel_event.type = WebKit::WebInputEvent::TouchCancel;
  cancel_event.timeStampSeconds = event->time_stamp().InSecondsF();
  render_widget_host_->ForwardTouchEvent(cancel_event);
  return true;
}

void RenderWidgetHostViewWin::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
}

void RenderWidgetHostViewWin::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
}

void RenderWidgetHostViewWin::SetCompositionText(
    const ui::CompositionText& composition) {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return;
  }
  if (!render_widget_host_)
     return;
  // ui::CompositionUnderline should be identical to
  // WebKit::WebCompositionUnderline, so that we can do reinterpret_cast safely.
  COMPILE_ASSERT(sizeof(ui::CompositionUnderline) ==
                 sizeof(WebKit::WebCompositionUnderline),
                 ui_CompositionUnderline__WebKit_WebCompositionUnderline_diff);
  const std::vector<WebKit::WebCompositionUnderline>& underlines =
      reinterpret_cast<const std::vector<WebKit::WebCompositionUnderline>&>(
          composition.underlines);
  render_widget_host_->ImeSetComposition(composition.text, underlines,
                                         composition.selection.end(),
                                         composition.selection.end());
}

void RenderWidgetHostViewWin::ConfirmCompositionText()  {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return;
  }
  // TODO(nona): Implement this function.
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewWin::ClearCompositionText() {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return;
  }
  // TODO(nona): Implement this function.
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewWin::InsertText(const string16& text) {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return;
  }
  if (render_widget_host_)
    render_widget_host_->ImeConfirmComposition(text);
}

void RenderWidgetHostViewWin::InsertChar(char16 ch, int flags) {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return;
  }
  // TODO(nona): Implement this function.
  NOTIMPLEMENTED();
}

ui::TextInputType RenderWidgetHostViewWin::GetTextInputType() const {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return ui::TEXT_INPUT_TYPE_NONE;
  }
  return text_input_type_;
}

bool RenderWidgetHostViewWin::CanComposeInline() const {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return false;
  }
  // TODO(nona): Implement this function.
  NOTIMPLEMENTED();
  return false;
}

gfx::Rect RenderWidgetHostViewWin::GetCaretBounds() {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return gfx::Rect(0, 0, 0, 0);
  }
  RECT tmp_rect = caret_rect_.ToRECT();
  ClientToScreen(&tmp_rect);
  return gfx::Rect(tmp_rect);
}

bool RenderWidgetHostViewWin::GetCompositionCharacterBounds(
    uint32 index, gfx::Rect* rect) {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return false;
  }
  DCHECK(rect);
  if (index >= composition_character_bounds_.size())
    return false;
  RECT rec = composition_character_bounds_[index].ToRECT();
  ClientToScreen(&rec);
  *rect = gfx::Rect(rec);
  return true;
}

bool RenderWidgetHostViewWin::HasCompositionText() {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return false;
  }
  // TODO(nona): Implement this function.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewWin::GetTextRange(ui::Range* range) {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return false;
  }
  range->set_start(selection_text_offset_);
  range->set_end(selection_text_offset_ + selection_text_.length());
  return false;
}

bool RenderWidgetHostViewWin::GetCompositionTextRange(ui::Range* range) {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return false;
  }
  // TODO(nona): Implement this function.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewWin::GetSelectionRange(ui::Range* range) {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return false;
  }
  range->set_start(selection_range_.start());
  range->set_end(selection_range_.end());
  return false;
}

bool RenderWidgetHostViewWin::SetSelectionRange(const ui::Range& range) {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return false;
  }
  // TODO(nona): Implement this function.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewWin::DeleteRange(const ui::Range& range) {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return false;
  }
  // TODO(nona): Implement this function.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewWin::GetTextFromRange(const ui::Range& range,
                                               string16* text) {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return false;
  }
  ui::Range selection_text_range(selection_text_offset_,
      selection_text_offset_ + selection_text_.length());
  if (!selection_text_range.Contains(range)) {
    text->clear();
    return false;
  }
  if (selection_text_range.EqualsIgnoringDirection(range)) {
    *text = selection_text_;
  } else {
    *text = selection_text_.substr(
        range.GetMin() - selection_text_offset_,
        range.length());
  }
  return true;
}

void RenderWidgetHostViewWin::OnInputMethodChanged() {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return;
  }
  // TODO(nona): Implement this function.
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewWin::ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return false;
  }
  // TODO(nona): Implement this function.
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewWin::ExtendSelectionAndDelete(
    size_t before,
    size_t after) {
  if (!base::win::IsTsfAwareRequired()) {
    NOTREACHED();
    return;
  }
  if (!render_widget_host_)
    return;
  render_widget_host_->ExtendSelectionAndDelete(before, after);
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewWin, private:

LRESULT RenderWidgetHostViewWin::OnCreate(CREATESTRUCT* create_struct) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnCreate");
  // Call the WM_INPUTLANGCHANGE message handler to initialize the input locale
  // of a browser process.
  OnInputLangChange(0, 0);
  // Marks that window as supporting mouse-wheel messages rerouting so it is
  // scrolled when under the mouse pointer even if inactive.
  props_.push_back(ui::SetWindowSupportsRerouteMouseWheel(m_hWnd));

  bool touch_enabled = base::win::GetVersion() >= base::win::VERSION_WIN7 &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableTouchEvents);
  if (touch_enabled)
    SetToTouchMode();
  else
    SetToGestureMode();

  UpdateIMEState();

  return 0;
}

void RenderWidgetHostViewWin::OnActivate(UINT action, BOOL minimized,
                                         HWND window) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnActivate");
  // If the container is a popup, clicking elsewhere on screen should close the
  // popup.
  if (close_on_deactivate_ && action == WA_INACTIVE) {
    // Send a windows message so that any derived classes
    // will get a change to override the default handling
    SendMessage(WM_CANCELMODE);
  }
}

void RenderWidgetHostViewWin::OnDestroy() {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnDestroy");
  DetachPluginsHelper(m_hWnd);

  props_.clear();

  if (base::win::GetVersion() >= base::win::VERSION_WIN7 &&
      IsTouchWindow(m_hWnd, NULL)) {
    UnregisterTouchWindow(m_hWnd);
  }

  CleanupCompositorWindow();

  ResetTooltip();
  TrackMouseLeave(false);
}

void RenderWidgetHostViewWin::OnPaint(HDC unused_dc) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnPaint");
  if (!render_widget_host_)
    return;

  DCHECK(render_widget_host_->GetProcess()->HasConnection());

  // If the GPU process is rendering to a child window, compositing is
  // already triggered by damage to compositor_host_window_, so all we need to
  // do here is clear borders during resize.
  if (compositor_host_window_ &&
      render_widget_host_->is_accelerated_compositing_active()) {
    // We initialize paint_dc here so that BeginPaint()/EndPaint()
    // get called to validate the region.
    CPaintDC paint_dc(m_hWnd);
    RECT host_rect, child_rect;
    GetClientRect(&host_rect);
    if (::GetClientRect(compositor_host_window_, &child_rect) &&
        (child_rect.right < host_rect.right ||
         child_rect.bottom < host_rect.bottom)) {
      paint_dc.FillRect(&host_rect,
          reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    }
    return;
  }

  about_to_validate_and_paint_ = true;
  BackingStoreWin* backing_store = static_cast<BackingStoreWin*>(
      render_widget_host_->GetBackingStore(true));

  // We initialize |paint_dc| (and thus call BeginPaint()) after calling
  // GetBackingStore(), so that if it updates the invalid rect we'll catch the
  // changes and repaint them.
  about_to_validate_and_paint_ = false;

  // Grab the region to paint before creation of paint_dc since it clears the
  // damage region.
  base::win::ScopedGDIObject<HRGN> damage_region(CreateRectRgn(0, 0, 0, 0));
  GetUpdateRgn(damage_region, FALSE);

  if (compositor_host_window_ && hide_compositor_window_at_next_paint_) {
    ::ShowWindow(compositor_host_window_, SW_HIDE);
    hide_compositor_window_at_next_paint_ = false;
  }

  CPaintDC paint_dc(m_hWnd);

  gfx::Rect damaged_rect(paint_dc.m_ps.rcPaint);
  if (damaged_rect.IsEmpty())
    return;

  if (accelerated_surface_.get() &&
      render_widget_host_->is_accelerated_compositing_active()) {
    AcceleratedPaint(paint_dc.m_hDC);
    return;
  }

  if (backing_store) {
    gfx::Rect bitmap_rect(gfx::Point(), backing_store->size());

    bool manage_colors = BackingStoreWin::ColorManagementEnabled();
    if (manage_colors)
      SetICMMode(paint_dc.m_hDC, ICM_ON);

    // Blit only the damaged regions from the backing store.
    DWORD data_size = GetRegionData(damage_region, 0, NULL);
    scoped_array<char> region_data_buf;
    RGNDATA* region_data = NULL;
    RECT* region_rects = NULL;

    if (data_size) {
      region_data_buf.reset(new char[data_size]);
      region_data = reinterpret_cast<RGNDATA*>(region_data_buf.get());
      region_rects = reinterpret_cast<RECT*>(region_data->Buffer);
      data_size = GetRegionData(damage_region, data_size, region_data);
    }

    if (!data_size) {
      // Grabbing the damaged regions failed, fake with the whole rect.
      data_size = sizeof(RGNDATAHEADER) + sizeof(RECT);
      region_data_buf.reset(new char[data_size]);
      region_data = reinterpret_cast<RGNDATA*>(region_data_buf.get());
      region_rects = reinterpret_cast<RECT*>(region_data->Buffer);
      region_data->rdh.nCount = 1;
      region_rects[0] = damaged_rect.ToRECT();
    }

    for (DWORD i = 0; i < region_data->rdh.nCount; ++i) {
      gfx::Rect paint_rect = bitmap_rect.Intersect(gfx::Rect(region_rects[i]));
      if (!paint_rect.IsEmpty()) {
        BitBlt(paint_dc.m_hDC,
               paint_rect.x(),
               paint_rect.y(),
               paint_rect.width(),
               paint_rect.height(),
               backing_store->hdc(),
               paint_rect.x(),
               paint_rect.y(),
               SRCCOPY);
      }
    }

    if (manage_colors)
      SetICMMode(paint_dc.m_hDC, ICM_OFF);

    // Fill the remaining portion of the damaged_rect with the background
    if (damaged_rect.right() > bitmap_rect.right()) {
      RECT r;
      r.left = std::max(bitmap_rect.right(), damaged_rect.x());
      r.right = damaged_rect.right();
      r.top = damaged_rect.y();
      r.bottom = std::min(bitmap_rect.bottom(), damaged_rect.bottom());
      DrawBackground(r, &paint_dc);
    }
    if (damaged_rect.bottom() > bitmap_rect.bottom()) {
      RECT r;
      r.left = damaged_rect.x();
      r.right = damaged_rect.right();
      r.top = std::max(bitmap_rect.bottom(), damaged_rect.y());
      r.bottom = damaged_rect.bottom();
      DrawBackground(r, &paint_dc);
    }
    if (!whiteout_start_time_.is_null()) {
      TimeDelta whiteout_duration = TimeTicks::Now() - whiteout_start_time_;
      UMA_HISTOGRAM_TIMES("MPArch.RWHH_WhiteoutDuration", whiteout_duration);

      // Reset the start time to 0 so that we start recording again the next
      // time the backing store is NULL...
      whiteout_start_time_ = TimeTicks();
    }
    if (!web_contents_switch_paint_time_.is_null()) {
      TimeDelta web_contents_switch_paint_duration = TimeTicks::Now() -
          web_contents_switch_paint_time_;
      UMA_HISTOGRAM_TIMES("MPArch.RWH_TabSwitchPaintDuration",
          web_contents_switch_paint_duration);
      // Reset contents_switch_paint_time_ to 0 so future tab selections are
      // recorded.
      web_contents_switch_paint_time_ = TimeTicks();
    }
  } else {
    DrawBackground(paint_dc.m_ps.rcPaint, &paint_dc);
    if (whiteout_start_time_.is_null())
      whiteout_start_time_ = TimeTicks::Now();
  }
}

void RenderWidgetHostViewWin::DrawBackground(const RECT& dirty_rect,
                                             CPaintDC* dc) {
  if (!background_.empty()) {
    gfx::Rect dirty_area(dirty_rect);
    gfx::Canvas canvas(dirty_area.size(), ui::SCALE_FACTOR_100P, true);
    canvas.Translate(gfx::Point().Subtract(dirty_area.origin()));

    gfx::Rect dc_rect(dc->m_ps.rcPaint);
    canvas.TileImageInt(background_, 0, 0, dc_rect.width(), dc_rect.height());

    skia::DrawToNativeContext(canvas.sk_canvas(), *dc, dirty_area.x(),
                              dirty_area.y(), NULL);
  } else {
    HBRUSH white_brush = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    dc->FillRect(&dirty_rect, white_brush);
  }
}

void RenderWidgetHostViewWin::OnNCPaint(HRGN update_region) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnNCPaint");
  // Do nothing.  This suppresses the resize corner that Windows would
  // otherwise draw for us.
}

void RenderWidgetHostViewWin::SetClickthroughRegion(SkRegion* region) {
  transparent_region_.reset(region);
}

LRESULT RenderWidgetHostViewWin::OnNCHitTest(const CPoint& point) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnNCHitTest");
  RECT rc;
  GetWindowRect(&rc);
  if (transparent_region_.get() &&
      transparent_region_->contains(point.x - rc.left, point.y - rc.top)) {
    SetMsgHandled(TRUE);
    return HTTRANSPARENT;
  }
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT RenderWidgetHostViewWin::OnEraseBkgnd(HDC dc) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnEraseBkgnd");
  return 1;
}

LRESULT RenderWidgetHostViewWin::OnSetCursor(HWND window, UINT hittest_code,
                                             UINT mouse_message_id) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnSetCursor");
  UpdateCursorIfOverSelf();
  return 0;
}

void RenderWidgetHostViewWin::OnSetFocus(HWND window) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnSetFocus");
  if (!render_widget_host_)
    return;

  if (GetBrowserAccessibilityManager())
    GetBrowserAccessibilityManager()->GotFocus();

  render_widget_host_->GotFocus();
  render_widget_host_->SetActive(true);

  if (base::win::IsTsfAwareRequired())
    ui::TsfBridge::GetInstance()->SetFocusedClient(m_hWnd, this);
}

void RenderWidgetHostViewWin::OnKillFocus(HWND window) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnKillFocus");
  if (!render_widget_host_)
    return;

  render_widget_host_->SetActive(false);
  render_widget_host_->Blur();

  if (base::win::IsTsfAwareRequired())
    ui::TsfBridge::GetInstance()->RemoveFocusedClient(this);
}

void RenderWidgetHostViewWin::OnCaptureChanged(HWND window) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnCaptureChanged");
  if (render_widget_host_)
    render_widget_host_->LostCapture();
}

void RenderWidgetHostViewWin::OnCancelMode() {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnCancelMode");
  if (render_widget_host_)
    render_widget_host_->LostCapture();

  if ((is_fullscreen_ || close_on_deactivate_) &&
      !weak_factory_.HasWeakPtrs()) {
    // Dismiss popups and menus.  We do this asynchronously to avoid changing
    // activation within this callstack, which may interfere with another window
    // being activated.  We can synchronously hide the window, but we need to
    // not change activation while doing so.
    SetWindowPos(NULL, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE |
                 SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&RenderWidgetHostViewWin::ShutdownHost,
                   weak_factory_.GetWeakPtr()));
  }
}

void RenderWidgetHostViewWin::OnInputLangChange(DWORD character_set,
                                                HKL input_language_id) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnInputLangChange");
  // Send the given Locale ID to the ImeInput object and retrieves whether
  // or not the current input context has IMEs.
  // If the current input context has IMEs, a browser process has to send a
  // request to a renderer process that it needs status messages about
  // the focused edit control from the renderer process.
  // On the other hand, if the current input context does not have IMEs, the
  // browser process also has to send a request to the renderer process that
  // it does not need the status messages any longer.
  // To minimize the number of this notification request, we should check if
  // the browser process is actually retrieving the status messages (this
  // state is stored in ime_notification_) and send a request only if the
  // browser process has to update this status, its details are listed below:
  // * If a browser process is not retrieving the status messages,
  //   (i.e. ime_notification_ == false),
  //   send this request only if the input context does have IMEs,
  //   (i.e. ime_status == true);
  //   When it successfully sends the request, toggle its notification status,
  //   (i.e.ime_notification_ = !ime_notification_ = true).
  // * If a browser process is retrieving the status messages
  //   (i.e. ime_notification_ == true),
  //   send this request only if the input context does not have IMEs,
  //   (i.e. ime_status == false).
  //   When it successfully sends the request, toggle its notification status,
  //   (i.e.ime_notification_ = !ime_notification_ = false).
  // To analyze the above actions, we can optimize them into the ones
  // listed below:
  // 1 Sending a request only if ime_status_ != ime_notification_, and;
  // 2 Copying ime_status to ime_notification_ if it sends the request
  //   successfully (because Action 1 shows ime_status = !ime_notification_.)
  bool ime_status = ime_input_.SetInputLanguage();
  if (ime_status != ime_notification_) {
    if (render_widget_host_) {
      render_widget_host_->SetInputMethodActive(ime_status);
      ime_notification_ = ime_status;
    }
  }
}

void RenderWidgetHostViewWin::OnThemeChanged() {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnThemeChanged");
  if (!render_widget_host_)
    return;
  render_widget_host_->Send(new ViewMsg_ThemeChanged(
      render_widget_host_->GetRoutingID()));
}

LRESULT RenderWidgetHostViewWin::OnNotify(int w_param, NMHDR* header) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnNotify");
  if (tooltip_hwnd_ == NULL)
    return 0;

  switch (header->code) {
    case TTN_GETDISPINFO: {
      NMTTDISPINFOW* tooltip_info = reinterpret_cast<NMTTDISPINFOW*>(header);
      tooltip_info->szText[0] = L'\0';
      tooltip_info->lpszText = const_cast<WCHAR*>(tooltip_text_.c_str());
      ::SendMessage(
        tooltip_hwnd_, TTM_SETMAXTIPWIDTH, 0, kTooltipMaxWidthPixels);
      SetMsgHandled(TRUE);
      break;
    }
    case TTN_POP:
      tooltip_showing_ = false;
      SetMsgHandled(TRUE);
      break;
    case TTN_SHOW:
      // Tooltip shouldn't be shown when the mouse is locked.
      DCHECK(!mouse_locked_);
      tooltip_showing_ = true;
      SetMsgHandled(TRUE);
      break;
  }
  return 0;
}

LRESULT RenderWidgetHostViewWin::OnImeSetContext(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnImeSetContext");
  if (!render_widget_host_)
    return 0;

  // We need status messages about the focused input control from a
  // renderer process when:
  //   * the current input context has IMEs, and;
  //   * an application is activated.
  // This seems to tell we should also check if the current input context has
  // IMEs before sending a request, however, this WM_IME_SETCONTEXT is
  // fortunately sent to an application only while the input context has IMEs.
  // Therefore, we just start/stop status messages according to the activation
  // status of this application without checks.
  bool activated = (wparam == TRUE);
  if (render_widget_host_) {
    render_widget_host_->SetInputMethodActive(activated);
    ime_notification_ = activated;
  }

  if (ime_notification_)
    ime_input_.CreateImeWindow(m_hWnd);

  ime_input_.CleanupComposition(m_hWnd);
  return ime_input_.SetImeWindowStyle(
      m_hWnd, message, wparam, lparam, &handled);
}

LRESULT RenderWidgetHostViewWin::OnImeStartComposition(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnImeStartComposition");
  if (!render_widget_host_)
    return 0;

  // Reset the composition status and create IME windows.
  ime_input_.CreateImeWindow(m_hWnd);
  ime_input_.ResetComposition(m_hWnd);
  // We have to prevent WTL from calling ::DefWindowProc() because the function
  // calls ::ImmSetCompositionWindow() and ::ImmSetCandidateWindow() to
  // over-write the position of IME windows.
  handled = TRUE;
  return 0;
}

LRESULT RenderWidgetHostViewWin::OnImeComposition(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnImeComposition");
  if (!render_widget_host_)
    return 0;

  // At first, update the position of the IME window.
  ime_input_.UpdateImeWindow(m_hWnd);

  // ui::CompositionUnderline should be identical to
  // WebKit::WebCompositionUnderline, so that we can do reinterpret_cast safely.
  COMPILE_ASSERT(sizeof(ui::CompositionUnderline) ==
                 sizeof(WebKit::WebCompositionUnderline),
                 ui_CompositionUnderline__WebKit_WebCompositionUnderline_diff);

  // Retrieve the result string and its attributes of the ongoing composition
  // and send it to a renderer process.
  ui::CompositionText composition;
  if (ime_input_.GetResult(m_hWnd, lparam, &composition.text)) {
    render_widget_host_->ImeConfirmComposition(composition.text);
    ime_input_.ResetComposition(m_hWnd);
    // Fall though and try reading the composition string.
    // Japanese IMEs send a message containing both GCS_RESULTSTR and
    // GCS_COMPSTR, which means an ongoing composition has been finished
    // by the start of another composition.
  }
  // Retrieve the composition string and its attributes of the ongoing
  // composition and send it to a renderer process.
  if (ime_input_.GetComposition(m_hWnd, lparam, &composition)) {
    // TODO(suzhe): due to a bug of webkit, we can't use selection range with
    // composition string. See: https://bugs.webkit.org/show_bug.cgi?id=37788
    composition.selection = ui::Range(composition.selection.end());

    // TODO(suzhe): convert both renderer_host and renderer to use
    // ui::CompositionText.
    const std::vector<WebKit::WebCompositionUnderline>& underlines =
        reinterpret_cast<const std::vector<WebKit::WebCompositionUnderline>&>(
            composition.underlines);
    render_widget_host_->ImeSetComposition(
        composition.text, underlines,
        composition.selection.start(), composition.selection.end());
  }
  // We have to prevent WTL from calling ::DefWindowProc() because we do not
  // want for the IMM (Input Method Manager) to send WM_IME_CHAR messages.
  handled = TRUE;
  return 0;
}

LRESULT RenderWidgetHostViewWin::OnImeEndComposition(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnImeEndComposition");
  if (!render_widget_host_)
    return 0;

  if (ime_input_.is_composing()) {
    // A composition has been ended while there is an ongoing composition,
    // i.e. the ongoing composition has been canceled.
    // We need to reset the composition status both of the ImeInput object and
    // of the renderer process.
    render_widget_host_->ImeCancelComposition();
    ime_input_.ResetComposition(m_hWnd);
  }
  ime_input_.DestroyImeWindow(m_hWnd);
  // Let WTL call ::DefWindowProc() and release its resources.
  handled = FALSE;
  return 0;
}

LRESULT RenderWidgetHostViewWin::OnImeRequest(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnImeRequest");
  if (!render_widget_host_) {
    handled = FALSE;
    return 0;
  }

  // Should not receive WM_IME_REQUEST message, if IME is disabled.
  if (text_input_type_ == ui::TEXT_INPUT_TYPE_NONE ||
      text_input_type_ == ui::TEXT_INPUT_TYPE_PASSWORD) {
    handled = FALSE;
    return 0;
  }

  switch (wparam) {
    case IMR_RECONVERTSTRING:
      return OnReconvertString(reinterpret_cast<RECONVERTSTRING*>(lparam));
    case IMR_DOCUMENTFEED:
      return OnDocumentFeed(reinterpret_cast<RECONVERTSTRING*>(lparam));
    case IMR_QUERYCHARPOSITION:
      return OnQueryCharPosition(reinterpret_cast<IMECHARPOSITION*>(lparam));
    default:
      handled = FALSE;
      return 0;
  }
}

LRESULT RenderWidgetHostViewWin::OnMouseEvent(UINT message, WPARAM wparam,
                                              LPARAM lparam, BOOL& handled) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnMouseEvent");
  handled = TRUE;

  if (message == WM_MOUSELEAVE)
    ignore_mouse_movement_ = true;

  if (mouse_locked_) {
    HandleLockedMouseEvent(message, wparam, lparam);
    MoveCursorToCenterIfNecessary();
    return 0;
  }

  if (::IsWindow(tooltip_hwnd_)) {
    // Forward mouse events through to the tooltip window
    MSG msg;
    msg.hwnd = m_hWnd;
    msg.message = message;
    msg.wParam = wparam;
    msg.lParam = lparam;
    SendMessage(tooltip_hwnd_, TTM_RELAYEVENT, NULL,
                reinterpret_cast<LPARAM>(&msg));
  }

  // TODO(jcampan): I am not sure if we should forward the message to the
  // WebContentsImpl first in the case of popups.  If we do, we would need to
  // convert the click from the popup window coordinates to the WebContentsImpl'
  // window coordinates. For now we don't forward the message in that case to
  // address bug #907474.
  // Note: GetParent() on popup windows returns the top window and not the
  // parent the window was created with (the parent and the owner of the popup
  // is the first non-child view of the view that was specified to the create
  // call).  So the WebContentsImpl's window would have to be specified to the
  // RenderViewHostHWND as there is no way to retrieve it from the HWND.

  // Don't forward if the container is a popup or fullscreen widget.
  if (!is_fullscreen_ && !close_on_deactivate_) {
    switch (message) {
      case WM_LBUTTONDOWN:
      case WM_MBUTTONDOWN:
      case WM_RBUTTONDOWN:
        // Finish the ongoing composition whenever a mouse click happens.
        // It matches IE's behavior.
        ime_input_.CleanupComposition(m_hWnd);
        // Fall through.
      case WM_MOUSEMOVE:
      case WM_MOUSELEAVE: {
        // Give the WebContentsImpl first crack at the message. It may want to
        // prevent forwarding to the renderer if some higher level browser
        // functionality is invoked.
        LPARAM parent_msg_lparam = lparam;
        if (message != WM_MOUSELEAVE) {
          // For the messages except WM_MOUSELEAVE, before forwarding them to
          // parent window, we should adjust cursor position from client
          // coordinates in current window to client coordinates in its parent
          // window.
          CPoint cursor_pos(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
          ClientToScreen(&cursor_pos);
          GetParent().ScreenToClient(&cursor_pos);
          parent_msg_lparam = MAKELPARAM(cursor_pos.x, cursor_pos.y);
        }
        if (SendMessage(GetParent(), message, wparam, parent_msg_lparam) != 0) {
          TRACE_EVENT0("browser", "EarlyOut_SentToParent");
          return 1;
        }
      }
    }
  }

  if (message == WM_LBUTTONDOWN && GetBrowserAccessibilityManager())
    GetBrowserAccessibilityManager()->GotMouseDown();

  ForwardMouseEventToRenderer(message, wparam, lparam);
  return 0;
}

LRESULT RenderWidgetHostViewWin::OnKeyEvent(UINT message, WPARAM wparam,
                                            LPARAM lparam, BOOL& handled) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnKeyEvent");
  handled = TRUE;

  // When Escape is pressed, force fullscreen windows to close if necessary.
  if ((message == WM_KEYDOWN || message == WM_KEYUP) && wparam == VK_ESCAPE) {
    if (is_fullscreen_) {
      SendMessage(WM_CANCELMODE);
      return 0;
    }
  }

  // If we are a pop-up, forward tab related messages to our parent HWND, so
  // that we are dismissed appropriately and so that the focus advance in our
  // parent.
  // TODO(jcampan): http://b/issue?id=1192881 Could be abstracted in the
  //                FocusManager.
  if (close_on_deactivate_ &&
      (((message == WM_KEYDOWN || message == WM_KEYUP) && (wparam == VK_TAB)) ||
        (message == WM_CHAR && wparam == L'\t'))) {
    // First close the pop-up.
    SendMessage(WM_CANCELMODE);
    // Then move the focus by forwarding the tab key to the parent.
    return ::SendMessage(GetParent(), message, wparam, lparam);
  }

  if (!render_widget_host_)
    return 0;

  // Bug 1845: we need to update the text direction when a user releases
  // either a right-shift key or a right-control key after pressing both of
  // them. So, we just update the text direction while a user is pressing the
  // keys, and we notify the text direction when a user releases either of them.
  // Bug 9718: http://crbug.com/9718 To investigate IE and notepad, this
  // shortcut is enabled only on a PC having RTL keyboard layouts installed.
  // We should emulate them.
  if (ui::ImeInput::IsRTLKeyboardLayoutInstalled()) {
    if (message == WM_KEYDOWN) {
      if (wparam == VK_SHIFT) {
        base::i18n::TextDirection dir;
        if (ui::ImeInput::IsCtrlShiftPressed(&dir)) {
          render_widget_host_->UpdateTextDirection(
              dir == base::i18n::RIGHT_TO_LEFT ?
              WebKit::WebTextDirectionRightToLeft :
              WebKit::WebTextDirectionLeftToRight);
        }
      } else if (wparam != VK_CONTROL) {
        // Bug 9762: http://crbug.com/9762 A user pressed a key except shift
        // and control keys.
        // When a user presses a key while he/she holds control and shift keys,
        // we cancel sending an IPC message in NotifyTextDirection() below and
        // ignore succeeding UpdateTextDirection() calls while we call
        // NotifyTextDirection().
        // To cancel it, this call set a flag that prevents sending an IPC
        // message in NotifyTextDirection() only if we are going to send it.
        // It is harmless to call this function if we aren't going to send it.
        render_widget_host_->CancelUpdateTextDirection();
      }
    } else if (message == WM_KEYUP &&
               (wparam == VK_SHIFT || wparam == VK_CONTROL)) {
      // We send an IPC message only if we need to update the text direction.
      render_widget_host_->NotifyTextDirection();
    }
  }

  // Special processing for enter key: When user hits enter in omnibox
  // we change focus to render host after the navigation, so repeat WM_KEYDOWNs
  // and WM_KEYUP are going to render host, despite being initiated in other
  // window. This code filters out these messages.
  bool ignore_keyboard_event = false;
  if (wparam == VK_RETURN) {
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
      if (KF_REPEAT & HIWORD(lparam)) {
        // this is a repeated key
        if (!capture_enter_key_)
          ignore_keyboard_event = true;
      } else {
        capture_enter_key_ = true;
      }
    } else if (message == WM_KEYUP || message == WM_SYSKEYUP) {
      if (!capture_enter_key_)
        ignore_keyboard_event = true;
      capture_enter_key_ = false;
    } else {
      // Ignore all other keyboard events for the enter key if not captured.
      if (!capture_enter_key_)
        ignore_keyboard_event = true;
    }
  }

  if (render_widget_host_ && !ignore_keyboard_event) {
    MSG msg = { m_hWnd, message, wparam, lparam };
    render_widget_host_->ForwardKeyboardEvent(NativeWebKeyboardEvent(msg));
  }
  return 0;
}

LRESULT RenderWidgetHostViewWin::OnWheelEvent(UINT message, WPARAM wparam,
                                              LPARAM lparam, BOOL& handled) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnWheelEvent");
  // Forward the mouse-wheel message to the window under the mouse if it belongs
  // to us.
  if (message == WM_MOUSEWHEEL &&
      ui::RerouteMouseWheel(m_hWnd, wparam, lparam)) {
    handled = TRUE;
    return 0;
  }

  // Workaround for Thinkpad mousewheel driver. We get mouse wheel/scroll
  // messages even if we are not in the foreground. So here we check if
  // we have any owned popup windows in the foreground and dismiss them.
  if (m_hWnd != GetForegroundWindow()) {
    HWND toplevel_hwnd = ::GetAncestor(m_hWnd, GA_ROOT);
    EnumThreadWindows(
        GetCurrentThreadId(),
        DismissOwnedPopups,
        reinterpret_cast<LPARAM>(toplevel_hwnd));
  }

  // This is a bit of a hack, but will work for now since we don't want to
  // pollute this object with WebContentsImpl-specific functionality...
  bool handled_by_WebContentsImpl = false;
  if (!is_fullscreen_ && GetParent()) {
    // Use a special reflected message to break recursion. If we send
    // WM_MOUSEWHEEL, the focus manager subclass of web contents will
    // route it back here.
    MSG new_message = {0};
    new_message.hwnd = m_hWnd;
    new_message.message = message;
    new_message.wParam = wparam;
    new_message.lParam = lparam;

    handled_by_WebContentsImpl =
        !!::SendMessage(GetParent(), base::win::kReflectedMessage, 0,
                        reinterpret_cast<LPARAM>(&new_message));
  }

  if (!handled_by_WebContentsImpl && render_widget_host_) {
    render_widget_host_->ForwardWheelEvent(
        WebInputEventFactory::mouseWheelEvent(m_hWnd, message, wparam,
                                              lparam));
  }
  handled = TRUE;
  return 0;
}

WebTouchState::WebTouchState(const RenderWidgetHostViewWin* window)
    : window_(window) { }

size_t WebTouchState::UpdateTouchPoints(
    TOUCHINPUT* points, size_t count) {
  // First we reset all touch event state. This involves removing any released
  // touchpoints and marking the rest as stationary. After that we go through
  // and alter/add any touchpoints (from the touch input buffer) that we can
  // coalesce into a single message. The return value is the number of consumed
  // input message.
  WebKit::WebTouchPoint* point = touch_event_.touches;
  WebKit::WebTouchPoint* end = point + touch_event_.touchesLength;
  while (point < end) {
    if (point->state == WebKit::WebTouchPoint::StateReleased) {
      *point = *(--end);
      --touch_event_.touchesLength;
    } else {
      point->state = WebKit::WebTouchPoint::StateStationary;
      point++;
    }
  }
  touch_event_.changedTouchesLength = 0;

  // Consume all events of the same type and add them to the changed list.
  int last_type = 0;
  for (size_t i = 0; i < count; ++i) {
    unsigned int mapped_id = GetMappedTouch(points[i].dwID);

    WebKit::WebTouchPoint* point = NULL;
    for (unsigned j = 0; j < touch_event_.touchesLength; ++j) {
      if (static_cast<DWORD>(touch_event_.touches[j].id) == mapped_id) {
        point =  &touch_event_.touches[j];
        break;
      }
    }

    // Use a move instead if we see a down on a point we already have.
    int type = GetTouchType(points[i]);
    if (point && type == TOUCHEVENTF_DOWN)
      SetTouchType(&points[i], TOUCHEVENTF_MOVE);

    // Stop processing when the event type changes.
    if (touch_event_.changedTouchesLength && type != last_type)
      return i;

    touch_event_.timeStampSeconds = points[i].dwTime / 1000.0;

    last_type = type;
    switch (type) {
      case TOUCHEVENTF_DOWN: {
        if (!(point = AddTouchPoint(&points[i])))
          continue;
        touch_event_.type = WebKit::WebInputEvent::TouchStart;
        break;
      }

      case TOUCHEVENTF_UP: {
        if (!point)  // Just throw away a stray up.
          continue;
        point->state = WebKit::WebTouchPoint::StateReleased;
        UpdateTouchPoint(point, &points[i]);
        touch_event_.type = WebKit::WebInputEvent::TouchEnd;
        break;
      }

      case TOUCHEVENTF_MOVE: {
        if (point) {
          point->state = WebKit::WebTouchPoint::StateMoved;
          // Don't update the message if the point didn't really move.
          if (UpdateTouchPoint(point, &points[i]))
            continue;
          touch_event_.type = WebKit::WebInputEvent::TouchMove;
        } else if (touch_event_.changedTouchesLength) {
          RemoveExpiredMappings();
          // Can't add a point if we're already handling move events.
          return i;
        } else {
          // Treat a move with no existing point as a down.
          if (!(point = AddTouchPoint(&points[i])))
            continue;
          last_type = TOUCHEVENTF_DOWN;
          SetTouchType(&points[i], TOUCHEVENTF_DOWN);
          touch_event_.type = WebKit::WebInputEvent::TouchStart;
        }
        break;
      }

      default:
        NOTREACHED();
        continue;
    }
    touch_event_.changedTouches[touch_event_.changedTouchesLength++] = *point;
  }

  RemoveExpiredMappings();
  return count;
}

void WebTouchState::RemoveExpiredMappings() {
  WebTouchState::MapType new_map;
  for (MapType::iterator it = touch_map_.begin();
      it != touch_map_.end();
      ++it) {
    WebKit::WebTouchPoint* point = touch_event_.touches;
    WebKit::WebTouchPoint* end = point + touch_event_.touchesLength;
    while (point < end) {
      if ((point->id == it->second) &&
          (point->state != WebKit::WebTouchPoint::StateReleased)) {
        new_map.insert(*it);
        break;
      }
      point++;
    }
  }
  touch_map_.swap(new_map);
}


bool WebTouchState::ReleaseTouchPoints() {
  if (touch_event_.touchesLength == 0)
    return false;
  // Mark every active touchpoint as released.
  touch_event_.type = WebKit::WebInputEvent::TouchEnd;
  touch_event_.changedTouchesLength = touch_event_.touchesLength;
  for (unsigned int i = 0; i < touch_event_.touchesLength; ++i) {
    touch_event_.touches[i].state = WebKit::WebTouchPoint::StateReleased;
    touch_event_.changedTouches[i].state =
        WebKit::WebTouchPoint::StateReleased;
  }

  return true;
}

WebKit::WebTouchPoint* WebTouchState::AddTouchPoint(
    TOUCHINPUT* touch_input) {
  DCHECK(touch_event_.touchesLength <
      WebKit::WebTouchEvent::touchesLengthCap);
  if (touch_event_.touchesLength >=
      WebKit::WebTouchEvent::touchesLengthCap)
    return NULL;
  WebKit::WebTouchPoint* point =
      &touch_event_.touches[touch_event_.touchesLength++];
  point->state = WebKit::WebTouchPoint::StatePressed;
  point->id = GetMappedTouch(touch_input->dwID);
  UpdateTouchPoint(point, touch_input);
  return point;
}

bool WebTouchState::UpdateTouchPoint(
    WebKit::WebTouchPoint* touch_point,
    TOUCHINPUT* touch_input) {
  CPoint coordinates(TOUCH_COORD_TO_PIXEL(touch_input->x),
                     TOUCH_COORD_TO_PIXEL(touch_input->y));
  int radius_x = 1;
  int radius_y = 1;
  if (touch_input->dwMask & TOUCHINPUTMASKF_CONTACTAREA) {
    radius_x = TOUCH_COORD_TO_PIXEL(touch_input->cxContact);
    radius_y = TOUCH_COORD_TO_PIXEL(touch_input->cyContact);
  }

  // Detect and exclude stationary moves.
  if (GetTouchType(*touch_input) == TOUCHEVENTF_MOVE &&
      touch_point->screenPosition.x == coordinates.x &&
      touch_point->screenPosition.y == coordinates.y &&
      touch_point->radiusX == radius_x &&
      touch_point->radiusY == radius_y) {
    touch_point->state = WebKit::WebTouchPoint::StateStationary;
    return true;
  }

  touch_point->screenPosition.x = coordinates.x;
  touch_point->screenPosition.y = coordinates.y;
  window_->GetParent().ScreenToClient(&coordinates);
  touch_point->position.x = coordinates.x;
  touch_point->position.y = coordinates.y;
  touch_point->radiusX = radius_x;
  touch_point->radiusY = radius_y;
  touch_point->force = 0;
  touch_point->rotationAngle = 0;
  return false;
}

// Find (or create) a mapping for _os_touch_id_.
unsigned int WebTouchState::GetMappedTouch(unsigned int os_touch_id) {
  MapType::iterator it = touch_map_.find(os_touch_id);
  if (it != touch_map_.end())
    return it->second;
  int next_value = 0;
  for (it = touch_map_.begin(); it != touch_map_.end(); ++it)
    next_value = std::max(next_value, it->second + 1);
  touch_map_[os_touch_id] = next_value;
  return next_value;
}

LRESULT RenderWidgetHostViewWin::OnTouchEvent(UINT message, WPARAM wparam,
                                              LPARAM lparam, BOOL& handled) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnTouchEvent");
  // TODO(jschuh): Add support for an arbitrary number of touchpoints.
  size_t total = std::min(static_cast<int>(LOWORD(wparam)),
      static_cast<int>(WebKit::WebTouchEvent::touchesLengthCap));
  TOUCHINPUT points[WebKit::WebTouchEvent::touchesLengthCap];

  if (!total || !GetTouchInputInfo((HTOUCHINPUT)lparam, total,
                                   points, sizeof(TOUCHINPUT))) {
    TRACE_EVENT0("browser", "EarlyOut_NothingToDo");
    return 0;
  }

  bool has_touch_handler = render_widget_host_->has_touch_handler() &&
      touch_events_enabled_;

  // Send a copy of the touch events on to the gesture recognizer.
  for (size_t start = 0; start < total;) {
    start += touch_state_->UpdateTouchPoints(points + start, total - start);
    if (has_touch_handler) {
      if (touch_state_->is_changed()) {
        render_widget_host_->ForwardTouchEvent(touch_state_->touch_event());
        touch_state_->QueueEvents(this, gesture_recognizer_.get());
      }
    } else {
      const WebKit::WebTouchEvent& touch_event = touch_state_->touch_event();
      base::TimeDelta timestamp = base::TimeDelta::FromMilliseconds(
          touch_event.timeStampSeconds * 1000);
      for (size_t i = 0; i < touch_event.touchesLength; ++i) {
        scoped_ptr<ui::GestureRecognizer::Gestures> gestures;
        gestures.reset(gesture_recognizer_->ProcessTouchEventForGesture(
            TouchEventFromWebTouchPoint(touch_event.touches[i], timestamp),
            ui::TOUCH_STATUS_UNKNOWN, this));
        ProcessGestures(gestures.get());
      }
    }
  }

  CloseTouchInputHandle((HTOUCHINPUT)lparam);

  return 0;
}

void RenderWidgetHostViewWin::ProcessGestures(
    ui::GestureRecognizer::Gestures* gestures) {
  if ((gestures == NULL) || gestures->empty())
    return;
  for (ui::GestureRecognizer::Gestures::iterator g_it = gestures->begin();
      g_it != gestures->end();
      ++g_it) {
    ForwardGestureEventToRenderer(*g_it);
  }
}

LRESULT RenderWidgetHostViewWin::OnMouseActivate(UINT message,
                                                 WPARAM wparam,
                                                 LPARAM lparam,
                                                 BOOL& handled) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnMouseActivate");
  if (!render_widget_host_)
    return MA_NOACTIVATE;

  if (!IsActivatable())
    return MA_NOACTIVATE;

  HWND focus_window = GetFocus();
  if (!::IsWindow(focus_window) || !IsChild(focus_window)) {
    // We handle WM_MOUSEACTIVATE to set focus to the underlying plugin
    // child window. This is to ensure that keyboard events are received
    // by the plugin. The correct way to fix this would be send over
    // an event to the renderer which would then eventually send over
    // a setFocus call to the plugin widget. This would ensure that
    // the renderer (webkit) knows about the plugin widget receiving
    // focus.
    // TODO(iyengar) Do the right thing as per the above comment.
    POINT cursor_pos = {0};
    ::GetCursorPos(&cursor_pos);
    ::ScreenToClient(m_hWnd, &cursor_pos);
    HWND child_window = ::RealChildWindowFromPoint(m_hWnd, cursor_pos);
    if (::IsWindow(child_window) && child_window != m_hWnd) {
      if (ui::GetClassName(child_window) ==
              webkit::npapi::kWrapperNativeWindowClassName)
        child_window = ::GetWindow(child_window, GW_CHILD);

      ::SetFocus(child_window);
      return MA_NOACTIVATE;
    }
  }
  handled = FALSE;
  render_widget_host_->OnPointerEventActivate();
  return MA_ACTIVATE;
}

LRESULT RenderWidgetHostViewWin::OnGestureEvent(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnGestureEvent");

  // Note that as of M22, touch events are enabled by default on Windows.
  // This code should not be reachable.
  DCHECK(!touch_events_enabled_);
  handled = FALSE;

  GESTUREINFO gi = {sizeof(GESTUREINFO)};
  HGESTUREINFO gi_handle = reinterpret_cast<HGESTUREINFO>(lparam);
  if (!::GetGestureInfo(gi_handle, &gi)) {
    DWORD error = GetLastError();
    NOTREACHED() << "Unable to get gesture info. Error : " << error;
    return 0;
  }

  if (gi.dwID == GID_ZOOM) {
    PageZoom zoom = PAGE_ZOOM_RESET;
    POINT zoom_center = {0};
    if (DecodeZoomGesture(m_hWnd, gi, &zoom, &zoom_center)) {
      handled = TRUE;
      Send(new ViewMsg_ZoomFactor(render_widget_host_->GetRoutingID(),
                                  zoom, zoom_center.x, zoom_center.y));
    }
  } else if (gi.dwID == GID_PAN) {
    // Right now we only decode scroll gestures and we forward to the page
    // as scroll events.
    POINT start;
    POINT delta;
    if (DecodeScrollGesture(gi, &start, &delta)) {
      handled = TRUE;
      render_widget_host_->ForwardWheelEvent(
          MakeFakeScrollWheelEvent(m_hWnd, start, delta));
    }
  }
  ::CloseGestureInfoHandle(gi_handle);
  return 0;
}

void RenderWidgetHostViewWin::OnAccessibilityNotifications(
    const std::vector<AccessibilityHostMsg_NotificationParams>& params) {
  if (!GetBrowserAccessibilityManager()) {
    SetBrowserAccessibilityManager(
        BrowserAccessibilityManager::CreateEmptyDocument(
            m_hWnd,
            static_cast<AccessibilityNodeData::State>(0),
            this));
  }
  GetBrowserAccessibilityManager()->OnAccessibilityNotifications(params);
}

bool RenderWidgetHostViewWin::LockMouse() {
  if (mouse_locked_)
    return true;

  mouse_locked_ = true;

  // Hide the tooltip window if it is currently visible. When the mouse is
  // locked, no mouse message is relayed to the tooltip window, so we don't need
  // to worry that it will reappear.
  if (::IsWindow(tooltip_hwnd_) && tooltip_showing_) {
    ::SendMessage(tooltip_hwnd_, TTM_POP, 0, 0);
    // Sending a TTM_POP message doesn't seem to actually hide the tooltip
    // window, although we will receive a TTN_POP notification. As a result,
    // ShowWindow() is explicitly called to hide the window.
    ::ShowWindow(tooltip_hwnd_, SW_HIDE);
  }

  // TODO(yzshen): ShowCursor(FALSE) causes SetCursorPos() to be ignored on
  // Remote Desktop.
  ::ShowCursor(FALSE);

  move_to_center_request_.pending = false;
  last_mouse_position_.locked_global = last_mouse_position_.unlocked_global;
  MoveCursorToCenterIfNecessary();

  CRect rect;
  GetWindowRect(&rect);
  ::ClipCursor(&rect);

  return true;
}

void RenderWidgetHostViewWin::UnlockMouse() {
  if (!mouse_locked_)
    return;

  mouse_locked_ = false;

  ::ClipCursor(NULL);
  ::SetCursorPos(last_mouse_position_.unlocked_global.x(),
                 last_mouse_position_.unlocked_global.y());
  ::ShowCursor(TRUE);

  if (render_widget_host_)
    render_widget_host_->LostMouseLock();
}

void RenderWidgetHostViewWin::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == NOTIFICATION_RENDERER_PROCESS_TERMINATED ||
         type == NOTIFICATION_FOCUS_CHANGED_IN_PAGE);

  if (type == NOTIFICATION_FOCUS_CHANGED_IN_PAGE) {
    if (pointer_down_context_)
      received_focus_change_after_pointer_down_ = true;
    focus_on_editable_field_ = *Details<bool>(details).ptr();
    if (virtual_keyboard_)
      DisplayOnScreenKeyboardIfNeeded();
  } else {
    // Get the RenderProcessHost that posted this notification, and exit
    // if it's not the one associated with this host view.
    RenderProcessHost* render_process_host =
        Source<RenderProcessHost>(source).ptr();
    DCHECK(render_process_host);
    if (!render_widget_host_ ||
        render_process_host != render_widget_host_->GetProcess())
      return;

    // If it was our RenderProcessHost that posted the notification,
    // clear the BrowserAccessibilityManager, because the renderer is
    // dead and any accessibility information we have is now stale.
    SetBrowserAccessibilityManager(NULL);
  }
}

static void PaintCompositorHostWindow(HWND hWnd) {
  PAINTSTRUCT paint;
  BeginPaint(hWnd, &paint);

  RenderWidgetHostViewWin* win = static_cast<RenderWidgetHostViewWin*>(
      ui::GetWindowUserData(hWnd));
  // Trigger composite to rerender window.
  if (win)
    win->AcceleratedPaint(paint.hdc);

  EndPaint(hWnd, &paint);
}

// WndProc for the compositor host window. We use this instead of Default so
// we can drop WM_PAINT and WM_ERASEBKGD messages on the floor.
static LRESULT CALLBACK CompositorHostWindowProc(HWND hWnd, UINT message,
                                                 WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_ERASEBKGND:
    return 0;
  case WM_DESTROY:
    ui::SetWindowUserData(hWnd, NULL);
    return 0;
  case WM_PAINT:
    PaintCompositorHostWindow(hWnd);
    return 0;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
}

void RenderWidgetHostViewWin::AcceleratedPaint(HDC dc) {
  // If we have a previous frame then present it immediately. Otherwise request
  // a new frame be composited.
  if (!accelerated_surface_.get() ||
      !accelerated_surface_->Present(dc)) {
    if (render_widget_host_)
      render_widget_host_->ScheduleComposite();
  }
}

// Creates a HWND within the RenderWidgetHostView that will serve as a host
// for a HWND that the GPU process will create. The host window is used
// to Z-position the GPU's window relative to other plugin windows.
gfx::GLSurfaceHandle RenderWidgetHostViewWin::GetCompositingSurface() {
  // If the window has been created, don't recreate it a second time
  if (compositor_host_window_)
    return gfx::GLSurfaceHandle(compositor_host_window_, true);

  // On Vista and later we present directly to the view window rather than a
  // child window.
  if (GpuDataManagerImpl::GetInstance()->IsUsingAcceleratedSurface()) {
    if (!accelerated_surface_.get())
      accelerated_surface_.reset(new AcceleratedSurface(m_hWnd));
    return gfx::GLSurfaceHandle(m_hWnd, true);
  }

  // On XP we need a child window that can be resized independently of the
  // parent.
  static ATOM atom = 0;
  static HMODULE instance = NULL;
  if (!atom) {
    WNDCLASSEX window_class;
    base::win::InitializeWindowClass(
        L"CompositorHostWindowClass",
        &base::win::WrappedWindowProc<CompositorHostWindowProc>,
        0, 0, 0, NULL, NULL, NULL, NULL, NULL,
        &window_class);
    instance = window_class.hInstance;
    atom = RegisterClassEx(&window_class);
    DCHECK(atom);
  }

  RECT currentRect;
  GetClientRect(&currentRect);

  // Ensure window does not have zero area because D3D cannot create a zero
  // area swap chain.
  int width = std::max(1,
      static_cast<int>(currentRect.right - currentRect.left));
  int height = std::max(1,
      static_cast<int>(currentRect.bottom - currentRect.top));

  compositor_host_window_ = CreateWindowEx(
    WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR,
    MAKEINTATOM(atom), 0,
    WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_DISABLED,
    0, 0, width, height, m_hWnd, 0, instance, 0);
  ui::CheckWindowCreated(compositor_host_window_);

  ui::SetWindowUserData(compositor_host_window_, this);

  gfx::GLSurfaceHandle surface_handle(compositor_host_window_, true);

  return surface_handle;
}

void RenderWidgetHostViewWin::OnAcceleratedCompositingStateChange() {
  bool show = render_widget_host_->is_accelerated_compositing_active();
  // When we first create the compositor, we will get a show request from
  // the renderer before we have gotten the create request from the GPU. In this
  // case, simply ignore the show request.
  if (compositor_host_window_ == NULL)
    return;

  if (show) {
    ::ShowWindow(compositor_host_window_, SW_SHOW);

    // Get all the child windows of this view, including the compositor window.
    std::vector<HWND> all_child_windows;
    ::EnumChildWindows(m_hWnd, AddChildWindowToVector,
        reinterpret_cast<LPARAM>(&all_child_windows));

    // Build a list of just the plugin window handles
    std::vector<HWND> plugin_windows;
    bool compositor_host_window_found = false;
    for (size_t i = 0; i < all_child_windows.size(); ++i) {
      if (all_child_windows[i] != compositor_host_window_)
        plugin_windows.push_back(all_child_windows[i]);
      else
        compositor_host_window_found = true;
    }
    DCHECK(compositor_host_window_found);

    // Set all the plugin windows to be "after" the compositor window.
    // When the compositor window is created, gets placed above plugins.
    for (size_t i = 0; i < plugin_windows.size(); ++i) {
      HWND next;
      if (i + 1 < plugin_windows.size())
        next = plugin_windows[i+1];
      else
        next = compositor_host_window_;
      ::SetWindowPos(plugin_windows[i], next, 0, 0, 0, 0,
          SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
    }
  } else {
    // Drop the backing store for the accelerated surface when the accelerated
    // compositor is disabled. Otherwise, a flash of the last presented frame
    // could appear when it is next enabled.
    if (accelerated_surface_.get())
      accelerated_surface_->Suspend();
    hide_compositor_window_at_next_paint_ = true;
  }
}

void RenderWidgetHostViewWin::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
  NOTREACHED();
}

void RenderWidgetHostViewWin::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
    int gpu_host_id) {
  NOTREACHED();
}

void RenderWidgetHostViewWin::AcceleratedSurfaceSuspend() {
    if (!accelerated_surface_.get())
      return;

    accelerated_surface_->Suspend();
}

bool RenderWidgetHostViewWin::HasAcceleratedSurface(
      const gfx::Size& desired_size) {
  // TODO(jbates) Implement this so this view can use GetBackingStore for both
  // software and GPU frames. Defaulting to false just makes GetBackingStore
  // only useable for software frames.
  return false;
}

void RenderWidgetHostViewWin::SetAccessibilityFocus(int acc_obj_id) {
  if (!render_widget_host_)
    return;

  render_widget_host_->AccessibilitySetFocus(acc_obj_id);
}

void RenderWidgetHostViewWin::AccessibilityDoDefaultAction(int acc_obj_id) {
  if (!render_widget_host_)
    return;

  render_widget_host_->AccessibilityDoDefaultAction(acc_obj_id);
}

void RenderWidgetHostViewWin::AccessibilityScrollToMakeVisible(
    int acc_obj_id, gfx::Rect subfocus) {
  if (!render_widget_host_)
    return;

  render_widget_host_->AccessibilityScrollToMakeVisible(acc_obj_id, subfocus);
}

void RenderWidgetHostViewWin::AccessibilityScrollToPoint(
    int acc_obj_id, gfx::Point point) {
  if (!render_widget_host_)
    return;

  render_widget_host_->AccessibilityScrollToPoint(acc_obj_id, point);
}

void RenderWidgetHostViewWin::AccessibilitySetTextSelection(
    int acc_obj_id, int start_offset, int end_offset) {
  if (!render_widget_host_)
    return;

  render_widget_host_->AccessibilitySetTextSelection(
      acc_obj_id, start_offset, end_offset);
}

LRESULT RenderWidgetHostViewWin::OnGetObject(UINT message, WPARAM wparam,
                                             LPARAM lparam, BOOL& handled) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnGetObject");
  if (kIdCustom == lparam) {
    // An MSAA client requestes our custom id. Assume that we have detected an
    // active windows screen reader.
    BrowserAccessibilityState::GetInstance()->OnScreenReaderDetected();
    render_widget_host_->SetAccessibilityMode(
        BrowserAccessibilityStateImpl::GetInstance()->GetAccessibilityMode());

    // Return with failure.
    return static_cast<LRESULT>(0L);
  }

  if (lparam != OBJID_CLIENT) {
    handled = false;
    return static_cast<LRESULT>(0L);
  }

  IAccessible* iaccessible = GetNativeViewAccessible();
  if (iaccessible)
    return LresultFromObject(IID_IAccessible, wparam, iaccessible);

  handled = false;
  return static_cast<LRESULT>(0L);
}

LRESULT RenderWidgetHostViewWin::OnParentNotify(UINT message, WPARAM wparam,
                                                LPARAM lparam, BOOL& handled) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnParentNotify");
  handled = FALSE;

  if (!render_widget_host_)
    return 0;

  switch (LOWORD(wparam)) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
      render_widget_host_->StartUserGesture();
      break;
    default:
      break;
  }
  return 0;
}

LRESULT RenderWidgetHostViewWin::OnPointerMessage(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnPointerMessage");
  POINT point = {0};

  point.x = GET_X_LPARAM(lparam);
  point.y = GET_Y_LPARAM(lparam);
  ScreenToClient(&point);

  lparam = MAKELPARAM(point.x, point.y);

  if (message == WM_POINTERDOWN) {
    if (!base::win::IsMetroProcess()) {
      SetFocus();
      pointer_down_context_ = true;
      received_focus_change_after_pointer_down_ = false;
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          base::Bind(&RenderWidgetHostViewWin::ResetPointerDownContext,
                     weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kPointerDownContextResetDelay));
    }
  }
  handled = FALSE;
  return 0;
}

void RenderWidgetHostViewWin::OnFinalMessage(HWND window) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::OnFinalMessage");
  // When the render widget host is being destroyed, it ends up calling
  // Destroy() which NULLs render_widget_host_.
  // Note: the following bug http://crbug.com/24248 seems to report that
  // OnFinalMessage is called with a deleted |render_widget_host_|. It is not
  // clear how this could happen, hence the NULLing of render_widget_host_
  // above.
  if (!render_widget_host_ && !being_destroyed_) {
    // If you hit this NOTREACHED, please add a comment to report it on
    // http://crbug.com/24248, including what you did when it happened and if
    // you can repro.
    NOTREACHED();
  }
  if (render_widget_host_)
    render_widget_host_->ViewDestroyed();
  delete this;
}

void RenderWidgetHostViewWin::TrackMouseLeave(bool track) {
  if (track == track_mouse_leave_)
    return;
  track_mouse_leave_ = track;

  DCHECK(m_hWnd);

  TRACKMOUSEEVENT tme;
  tme.cbSize = sizeof(TRACKMOUSEEVENT);
  tme.dwFlags = TME_LEAVE;
  if (!track_mouse_leave_)
    tme.dwFlags |= TME_CANCEL;
  tme.hwndTrack = m_hWnd;

  TrackMouseEvent(&tme);
}

bool RenderWidgetHostViewWin::Send(IPC::Message* message) {
  if (!render_widget_host_)
    return false;
  return render_widget_host_->Send(message);
}

void RenderWidgetHostViewWin::EnsureTooltip() {
  UINT message = TTM_NEWTOOLRECT;

  TOOLINFO ti = {0};
  ti.cbSize = sizeof(ti);
  ti.hwnd = m_hWnd;
  ti.uId = 0;
  if (!::IsWindow(tooltip_hwnd_)) {
    message = TTM_ADDTOOL;
    tooltip_hwnd_ = CreateWindowEx(
        WS_EX_TRANSPARENT | l10n_util::GetExtendedTooltipStyles(),
        TOOLTIPS_CLASS, NULL, TTS_NOPREFIX, 0, 0, 0, 0, m_hWnd, NULL,
        NULL, NULL);
    if (!tooltip_hwnd_) {
      // Tooltip creation can inexplicably fail. See bug 82913 for details.
      LOG_GETLASTERROR(WARNING) <<
          "Tooltip creation failed, tooltips won't work";
      return;
    }
    ti.uFlags = TTF_TRANSPARENT;
    ti.lpszText = LPSTR_TEXTCALLBACK;

    // Ensure web content tooltips are displayed for at least this amount of
    // time, to give users a chance to read longer messages.
    const int kMinimumAutopopDurationMs = 10 * 1000;
    int autopop_duration_ms =
        SendMessage(tooltip_hwnd_, TTM_GETDELAYTIME, TTDT_AUTOPOP, NULL);
    if (autopop_duration_ms < kMinimumAutopopDurationMs) {
      SendMessage(tooltip_hwnd_, TTM_SETDELAYTIME, TTDT_AUTOPOP,
                  kMinimumAutopopDurationMs);
    }
  }

  CRect cr;
  GetClientRect(&ti.rect);
  SendMessage(tooltip_hwnd_, message, NULL, reinterpret_cast<LPARAM>(&ti));
}

void RenderWidgetHostViewWin::ResetTooltip() {
  if (::IsWindow(tooltip_hwnd_))
    ::DestroyWindow(tooltip_hwnd_);
  tooltip_hwnd_ = NULL;
}

bool RenderWidgetHostViewWin::ForwardGestureEventToRenderer(
    ui::GestureEvent* gesture) {
  if (!render_widget_host_)
    return false;

  WebKit::WebGestureEvent web_gesture = CreateWebGestureEvent(m_hWnd, *gesture);
  if (web_gesture.type == WebKit::WebGestureEvent::Undefined)
    return false;
  render_widget_host_->ForwardGestureEvent(web_gesture);
  return true;
}

void RenderWidgetHostViewWin::ForwardMouseEventToRenderer(UINT message,
                                                          WPARAM wparam,
                                                          LPARAM lparam) {
  TRACE_EVENT0("browser",
               "RenderWidgetHostViewWin::ForwardMouseEventToRenderer");
  if (!render_widget_host_) {
    TRACE_EVENT0("browser", "EarlyOut_NoRWH");
    return;
  }

  WebMouseEvent event(
      WebInputEventFactory::mouseEvent(m_hWnd, message, wparam, lparam));

  if (mouse_locked_) {
    event.movementX = event.globalX - last_mouse_position_.locked_global.x();
    event.movementY = event.globalY - last_mouse_position_.locked_global.y();
    last_mouse_position_.locked_global.SetPoint(event.globalX, event.globalY);

    event.x = last_mouse_position_.unlocked.x();
    event.y = last_mouse_position_.unlocked.y();
    event.windowX = last_mouse_position_.unlocked.x();
    event.windowY = last_mouse_position_.unlocked.y();
    event.globalX = last_mouse_position_.unlocked_global.x();
    event.globalY = last_mouse_position_.unlocked_global.y();
  } else {
    if (ignore_mouse_movement_) {
      ignore_mouse_movement_ = false;
      event.movementX = 0;
      event.movementY = 0;
    } else {
      event.movementX =
          event.globalX - last_mouse_position_.unlocked_global.x();
      event.movementY =
          event.globalY - last_mouse_position_.unlocked_global.y();
    }

    last_mouse_position_.unlocked.SetPoint(event.windowX, event.windowY);
    last_mouse_position_.unlocked_global.SetPoint(event.globalX, event.globalY);
  }

  // Windows sends (fake) mouse messages for touch events. Don't send these to
  // the render widget.
  if (!touch_events_enabled_ || !ui::IsMouseEventFromTouch(message)) {
    // Send the event to the renderer before changing mouse capture, so that
    // the capturelost event arrives after mouseup.
    render_widget_host_->ForwardMouseEvent(event);
  }

  switch (event.type) {
    case WebInputEvent::MouseMove:
      TrackMouseLeave(true);
      break;
    case WebInputEvent::MouseLeave:
      TrackMouseLeave(false);
      break;
    case WebInputEvent::MouseDown:
      SetCapture();
      break;
    case WebInputEvent::MouseUp:
      if (GetCapture() == m_hWnd)
        ReleaseCapture();
      break;
  }

  if (IsActivatable() && event.type == WebInputEvent::MouseDown) {
    // This is a temporary workaround for bug 765011 to get focus when the
    // mouse is clicked. This happens after the mouse down event is sent to
    // the renderer because normally Windows does a WM_SETFOCUS after
    // WM_LBUTTONDOWN.
    SetFocus();
  }
}

void RenderWidgetHostViewWin::ShutdownHost() {
  weak_factory_.InvalidateWeakPtrs();
  if (render_widget_host_)
    render_widget_host_->Shutdown();
  // Do not touch any members at this point, |this| has been deleted.
}

void RenderWidgetHostViewWin::DoPopupOrFullscreenInit(HWND parent_hwnd,
                                                      const gfx::Rect& pos,
                                                      DWORD ex_style) {
  Create(parent_hwnd, NULL, NULL, WS_POPUP, ex_style);
  MoveWindow(pos.x(), pos.y(), pos.width(), pos.height(), TRUE);
  ShowWindow(IsActivatable() ? SW_SHOW : SW_SHOWNA);

  if (is_fullscreen_ && base::win::IsMetroProcess()) {
    MetroSetFrameWindow set_frame_window =
        reinterpret_cast<MetroSetFrameWindow>(
            ::GetProcAddress(base::win::GetMetroModule(), "SetFrameWindow"));
    DCHECK(set_frame_window);
    set_frame_window(m_hWnd);
  }
}

CPoint RenderWidgetHostViewWin::GetClientCenter() const {
  CRect rect;
  GetClientRect(&rect);
  return rect.CenterPoint();
}

void RenderWidgetHostViewWin::MoveCursorToCenterIfNecessary() {
  DCHECK(mouse_locked_);

  CRect rect;
  GetWindowRect(&rect);
  int border_x = rect.Width() * kMouseLockBorderPercentage / 100;
  int border_y = rect.Height() * kMouseLockBorderPercentage / 100;

  bool should_move =
      last_mouse_position_.locked_global.x() < rect.left + border_x ||
      last_mouse_position_.locked_global.x() > rect.right - border_x ||
      last_mouse_position_.locked_global.y() < rect.top + border_y ||
      last_mouse_position_.locked_global.y() > rect.bottom - border_y;

  if (should_move) {
    move_to_center_request_.pending = true;
    move_to_center_request_.target = rect.CenterPoint();
    if (!::SetCursorPos(move_to_center_request_.target.x(),
                        move_to_center_request_.target.y())) {
      LOG_GETLASTERROR(WARNING) << "Failed to set cursor position.";
    }
  }
}

void RenderWidgetHostViewWin::HandleLockedMouseEvent(UINT message,
                                                     WPARAM wparam,
                                                     LPARAM lparam) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewWin::HandleLockedMouseEvent");
  DCHECK(mouse_locked_);

  if (message == WM_MOUSEMOVE && move_to_center_request_.pending) {
    // Ignore WM_MOUSEMOVE messages generated by
    // MoveCursorToCenterIfNecessary().
    CPoint current_position(LOWORD(lparam), HIWORD(lparam));
    ClientToScreen(&current_position);
    if (move_to_center_request_.target.x() == current_position.x &&
        move_to_center_request_.target.y() == current_position.y) {
      move_to_center_request_.pending = false;
      last_mouse_position_.locked_global = move_to_center_request_.target;
      return;
    }
  }

  ForwardMouseEventToRenderer(message, wparam, lparam);
}

LRESULT RenderWidgetHostViewWin::OnDocumentFeed(RECONVERTSTRING* reconv) {
  size_t target_offset;
  size_t target_length;
  bool has_composition;
  if (!composition_range_.is_empty()) {
    target_offset = composition_range_.GetMin();
    target_length = composition_range_.length();
    has_composition = true;
  } else if (selection_range_.IsValid()) {
    target_offset = selection_range_.GetMin();
    target_length = selection_range_.length();
    has_composition = false;
  } else {
    return 0;
  }

  size_t len = selection_text_.length();
  size_t need_size = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);

  if (target_offset < selection_text_offset_ ||
      target_offset + target_length > selection_text_offset_ + len) {
    return 0;
  }

  if (!reconv)
    return need_size;

  if (reconv->dwSize < need_size)
    return 0;

  reconv->dwVersion = 0;
  reconv->dwStrLen = len;
  reconv->dwStrOffset = sizeof(RECONVERTSTRING);
  reconv->dwCompStrLen = has_composition ? target_length: 0;
  reconv->dwCompStrOffset =
      (target_offset - selection_text_offset_) * sizeof(WCHAR);
  reconv->dwTargetStrLen = target_length;
  reconv->dwTargetStrOffset = reconv->dwCompStrOffset;
  memcpy(reinterpret_cast<char*>(reconv) + sizeof(RECONVERTSTRING),
         selection_text_.c_str(), len * sizeof(WCHAR));

  // According to Microsft API document, IMR_RECONVERTSTRING and
  // IMR_DOCUMENTFEED should return reconv, but some applications return
  // need_size.
  return reinterpret_cast<LRESULT>(reconv);
}

LRESULT RenderWidgetHostViewWin::OnReconvertString(RECONVERTSTRING* reconv) {
  // If there is a composition string already, we don't allow reconversion.
  if (ime_input_.is_composing())
    return 0;

  if (selection_range_.is_empty())
    return 0;

  if (selection_text_.empty())
    return 0;

  if (selection_range_.GetMin() < selection_text_offset_ ||
      selection_range_.GetMax() >
      selection_text_offset_ + selection_text_.length()) {
    return 0;
  }

  size_t len = selection_range_.length();
  size_t need_size = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);

  if (!reconv)
    return need_size;

  if (reconv->dwSize < need_size)
    return 0;

  reconv->dwVersion = 0;
  reconv->dwStrLen = len;
  reconv->dwStrOffset = sizeof(RECONVERTSTRING);
  reconv->dwCompStrLen = len;
  reconv->dwCompStrOffset = 0;
  reconv->dwTargetStrLen = len;
  reconv->dwTargetStrOffset = 0;

  size_t offset = selection_range_.GetMin() - selection_text_offset_;
  memcpy(reinterpret_cast<char*>(reconv) + sizeof(RECONVERTSTRING),
         selection_text_.c_str() + offset, len * sizeof(WCHAR));

  // According to Microsft API document, IMR_RECONVERTSTRING and
  // IMR_DOCUMENTFEED should return reconv, but some applications return
  // need_size.
  return reinterpret_cast<LRESULT>(reconv);
}

LRESULT RenderWidgetHostViewWin::OnQueryCharPosition(
    IMECHARPOSITION* position) {
  DCHECK(position);

  if (!ime_input_.is_composing() || composition_range_.is_empty() ||
      position->dwSize < sizeof(IMECHARPOSITION) ||
      position->dwCharPos >= composition_character_bounds_.size()) {
    return 0;
  }

  RECT target_rect =
      composition_character_bounds_[position->dwCharPos].ToRECT();
  ClientToScreen(&target_rect);

  RECT document_rect = GetViewBounds().ToRECT();
  ClientToScreen(&document_rect);

  position->pt.x = target_rect.left;
  position->pt.y = target_rect.top;
  position->cLineHeight = target_rect.bottom - target_rect.top;
  position->rcDocument = document_rect;
  return 1;
}

void RenderWidgetHostViewWin::UpdateIMEState() {
  if (base::win::IsTsfAwareRequired()) {
    ui::TsfBridge::GetInstance()->OnTextInputTypeChanged(this);
    return;
  }
  if (text_input_type_ != ui::TEXT_INPUT_TYPE_NONE &&
      text_input_type_ != ui::TEXT_INPUT_TYPE_PASSWORD) {
    ime_input_.EnableIME(m_hWnd);
  } else {
    ime_input_.DisableIME(m_hWnd);
  }
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostView, public:

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewWin(widget);
}

void RenderWidgetHostViewWin::DisplayOnScreenKeyboardIfNeeded() {
  if (focus_on_editable_field_) {
    if (pointer_down_context_) {
      virtual_keyboard_->SetInPlaceVisibility(TRUE);
    }
  } else {
    virtual_keyboard_->SetInPlaceVisibility(FALSE);
  }
}

void RenderWidgetHostViewWin::ResetPointerDownContext() {
  // If the default focus on the page is on an edit field and we did not
  // receive a focus change in the context of a pointer down message, it means
  // that the pointer down message occurred on the edit field and we should
  // display the on screen keyboard
  if (!received_focus_change_after_pointer_down_ && virtual_keyboard_)
    DisplayOnScreenKeyboardIfNeeded();
  received_focus_change_after_pointer_down_ = false;
  pointer_down_context_ = false;
}

}  // namespace content
