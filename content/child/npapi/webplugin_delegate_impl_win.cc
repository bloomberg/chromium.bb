// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/webplugin_delegate_impl.h"

#include <stdint.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/version.h"
#include "base/win/windows_version.h"
#include "content/child/npapi/plugin_instance.h"
#include "content/child/npapi/plugin_lib.h"
#include "content/child/npapi/webplugin.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/plugin_constants_win.h"
#include "content/public/common/content_constants.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/win/dpi.h"
#include "ui/gfx/win/hwnd_util.h"

using blink::WebKeyboardEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;

namespace content {

namespace {

// http://crbug.com/16114
// Enforces providing a valid device context in NPWindow, so that NPP_SetWindow
// is never called with NPNWindoTypeDrawable and NPWindow set to NULL.
// Doing so allows removing NPP_SetWindow call during painting a windowless
// plugin, which otherwise could trigger layout change while painting by
// invoking NPN_Evaluate. Which would cause bad, bad crashes. Bad crashes.
// TODO(dglazkov): If this approach doesn't produce regressions, move class to
// webplugin_delegate_impl.h and implement for other platforms.
class DrawableContextEnforcer {
 public:
  explicit DrawableContextEnforcer(NPWindow* window)
      : window_(window),
        disposable_dc_(window && !window->window) {
    // If NPWindow is NULL, create a device context with monochrome 1x1 surface
    // and stuff it to NPWindow.
    if (disposable_dc_)
      window_->window = CreateCompatibleDC(NULL);
  }

  ~DrawableContextEnforcer() {
    if (!disposable_dc_)
      return;

    DeleteDC(static_cast<HDC>(window_->window));
    window_->window = NULL;
  }

 private:
  NPWindow* window_;
  bool disposable_dc_;
};

}  // namespace

WebPluginDelegateImpl::WebPluginDelegateImpl(WebPlugin* plugin,
                                             PluginInstance* instance)
    : plugin_(plugin),
      instance_(instance),
      quirks_(0),
      handle_event_depth_(0),
      first_set_window_call_(true),
      plugin_has_focus_(false),
      has_webkit_focus_(false),
      containing_view_has_focus_(true),
      creation_succeeded_(false),
      user_gesture_msg_factory_(this) {
  memset(&window_, 0, sizeof(window_));
}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
  DestroyInstance();
}

bool WebPluginDelegateImpl::PlatformInitialize() {
  return true;
}

void WebPluginDelegateImpl::PlatformDestroyInstance() {
}

void WebPluginDelegateImpl::Paint(SkCanvas* canvas, const gfx::Rect& rect) {
  if (skia::SupportsPlatformPaint(canvas)) {
    skia::ScopedPlatformPaint scoped_platform_paint(canvas);
    HDC hdc = scoped_platform_paint.GetPlatformSurface();
    WindowlessPaint(hdc, rect);
  }
}

// Returns true if the message passed in corresponds to a user gesture.
static bool IsUserGestureMessage(unsigned int message) {
  switch (message) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
      return true;

    default:
      break;
  }

  return false;
}

void WebPluginDelegateImpl::WindowlessUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  bool window_rect_changed = (window_rect_ != window_rect);
  // Only resend to the instance if the geometry has changed.
  if (!window_rect_changed && clip_rect == clip_rect_)
    return;

  clip_rect_ = clip_rect;
  window_rect_ = window_rect;

  WindowlessSetWindow();

  if (window_rect_changed) {
    WINDOWPOS win_pos = {0};
    win_pos.x = window_rect_.x();
    win_pos.y = window_rect_.y();
    win_pos.cx = window_rect_.width();
    win_pos.cy = window_rect_.height();

    NPEvent pos_changed_event;
    pos_changed_event.event = WM_WINDOWPOSCHANGED;
    pos_changed_event.wParam = 0;
    pos_changed_event.lParam = reinterpret_cast<uintptr_t>(&win_pos);

    instance()->NPP_HandleEvent(&pos_changed_event);
  }
}

void WebPluginDelegateImpl::WindowlessPaint(HDC hdc,
                                            const gfx::Rect& damage_rect) {
  DCHECK(hdc);

  RECT damage_rect_win;
  damage_rect_win.left   = damage_rect.x();  // + window_rect_.x();
  damage_rect_win.top    = damage_rect.y();  // + window_rect_.y();
  damage_rect_win.right  = damage_rect_win.left + damage_rect.width();
  damage_rect_win.bottom = damage_rect_win.top + damage_rect.height();

  // Save away the old HDC as this could be a nested invocation.
  void* old_dc = window_.window;
  window_.window = hdc;

  NPEvent paint_event;
  paint_event.event = WM_PAINT;
  paint_event.wParam = PtrToUlong(hdc);
  paint_event.lParam = reinterpret_cast<uintptr_t>(&damage_rect_win);
  instance()->NPP_HandleEvent(&paint_event);
  window_.window = old_dc;
}

void WebPluginDelegateImpl::WindowlessSetWindow() {
  if (!instance())
    return;

  if (window_rect_.IsEmpty())  // wait for geometry to be set.
    return;

  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();
  window_.type = NPWindowTypeDrawable;
  DrawableContextEnforcer enforcer(&window_);

  NPError err = instance()->NPP_SetWindow(&window_);
  DCHECK(err == NPERR_NO_ERROR);
}

bool WebPluginDelegateImpl::PlatformSetPluginHasFocus(bool focused) {
  NPEvent focus_event;
  focus_event.event = focused ? WM_SETFOCUS : WM_KILLFOCUS;
  focus_event.wParam = 0;
  focus_event.lParam = 0;

  instance()->NPP_HandleEvent(&focus_event);
  return true;
}

static bool NPEventFromWebMouseEvent(const WebMouseEvent& event,
                                     NPEvent* np_event) {
  np_event->lParam =
      static_cast<uint32_t>(MAKELPARAM(event.windowX, event.windowY));
  np_event->wParam = 0;

  if (event.modifiers & WebInputEvent::ControlKey)
    np_event->wParam |= MK_CONTROL;
  if (event.modifiers & WebInputEvent::ShiftKey)
    np_event->wParam |= MK_SHIFT;
  if (event.modifiers & WebInputEvent::LeftButtonDown)
    np_event->wParam |= MK_LBUTTON;
  if (event.modifiers & WebInputEvent::MiddleButtonDown)
    np_event->wParam |= MK_MBUTTON;
  if (event.modifiers & WebInputEvent::RightButtonDown)
    np_event->wParam |= MK_RBUTTON;

  switch (event.type) {
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseLeave:
    case WebInputEvent::MouseEnter:
      np_event->event = WM_MOUSEMOVE;
      return true;
    case WebInputEvent::MouseDown:
      switch (event.button) {
        case WebMouseEvent::ButtonLeft:
          np_event->event = WM_LBUTTONDOWN;
          break;
        case WebMouseEvent::ButtonMiddle:
          np_event->event = WM_MBUTTONDOWN;
          break;
        case WebMouseEvent::ButtonRight:
          np_event->event = WM_RBUTTONDOWN;
          break;
        case WebMouseEvent::ButtonNone:
          break;
      }
      return true;
    case WebInputEvent::MouseUp:
      switch (event.button) {
        case WebMouseEvent::ButtonLeft:
          np_event->event = WM_LBUTTONUP;
          break;
        case WebMouseEvent::ButtonMiddle:
          np_event->event = WM_MBUTTONUP;
          break;
        case WebMouseEvent::ButtonRight:
          np_event->event = WM_RBUTTONUP;
          break;
        case WebMouseEvent::ButtonNone:
          break;
      }
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool NPEventFromWebKeyboardEvent(const WebKeyboardEvent& event,
                                        NPEvent* np_event) {
  np_event->wParam = event.windowsKeyCode;

  switch (event.type) {
    case WebInputEvent::KeyDown:
      np_event->event = WM_KEYDOWN;
      np_event->lParam = 0;
      return true;
    case WebInputEvent::Char:
      np_event->event = WM_CHAR;
      np_event->lParam = 0;
      return true;
    case WebInputEvent::KeyUp:
      np_event->event = WM_KEYUP;
      np_event->lParam = 0x8000;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool NPEventFromWebInputEvent(const WebInputEvent& event,
                                     NPEvent* np_event) {
  switch (event.type) {
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseLeave:
    case WebInputEvent::MouseEnter:
    case WebInputEvent::MouseDown:
    case WebInputEvent::MouseUp:
      if (event.size < sizeof(WebMouseEvent)) {
        NOTREACHED();
        return false;
      }
      return NPEventFromWebMouseEvent(
          *static_cast<const WebMouseEvent*>(&event), np_event);
    case WebInputEvent::KeyDown:
    case WebInputEvent::Char:
    case WebInputEvent::KeyUp:
      if (event.size < sizeof(WebKeyboardEvent)) {
        NOTREACHED();
        return false;
      }
      return NPEventFromWebKeyboardEvent(
          *static_cast<const WebKeyboardEvent*>(&event), np_event);
    default:
      return false;
  }
}

bool WebPluginDelegateImpl::PlatformHandleInputEvent(
    const WebInputEvent& event, WebCursor::CursorInfo* cursor_info) {
  DCHECK(cursor_info != NULL);

  NPEvent np_event;
  if (!NPEventFromWebInputEvent(event, &np_event)) {
    return false;
  }

  handle_event_depth_++;

  bool popups_enabled = false;

  if (IsUserGestureMessage(np_event.event)) {
    instance()->PushPopupsEnabledState(true);
    popups_enabled = true;
  }

  bool ret = instance()->NPP_HandleEvent(&np_event) != 0;

  if (popups_enabled) {
    instance()->PopPopupsEnabledState();
  }

  // Flash and SilverLight always return false, even when they swallow the
  // event.  Flash does this because it passes the event to its window proc,
  // which is supposed to return 0 if an event was handled.  There are few
  // exceptions, such as IME, where it sometimes returns true.
  ret = true;

  if (np_event.event == WM_MOUSEMOVE) {
    current_windowless_cursor_.InitFromExternalCursor(GetCursor());
    // Snag a reference to the current cursor ASAP in case the plugin modified
    // it. There is a nasty race condition here with the multiprocess browser
    // as someone might be setting the cursor in the main process as well.
    current_windowless_cursor_.GetCursorInfo(cursor_info);
  }

  handle_event_depth_--;

  return ret;
}

}  // namespace content
