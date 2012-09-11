// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_base.h"

#include "base/logging.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/port/browser/smooth_scroll_gesture.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

#if defined(OS_WIN)
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/win/wrapped_window_proc.h"
#include "content/browser/plugin_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/content_switches.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/gdi_util.h"
#include "webkit/plugins/npapi/plugin_constants_win.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"
#endif

#if defined(TOOLKIT_GTK)
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "content/browser/renderer_host/gtk_window_utils.h"
#endif

namespace content {

// How long a smooth scroll gesture should run when it is a near scroll.
static const int64 kDurationOfNearScrollGestureMs = 150;

// How long a smooth scroll gesture should run when it is a far scroll.
static const int64 kDurationOfFarScrollGestureMs = 500;

// static
RenderWidgetHostViewPort* RenderWidgetHostViewPort::FromRWHV(
    RenderWidgetHostView* rwhv) {
  return static_cast<RenderWidgetHostViewPort*>(rwhv);
}

// static
RenderWidgetHostViewPort* RenderWidgetHostViewPort::CreateViewForWidget(
    content::RenderWidgetHost* widget) {
  return FromRWHV(RenderWidgetHostView::CreateViewForWidget(widget));
}

#if defined(OS_WIN)

namespace {

// |window| is the plugin HWND, created and destroyed in the plugin process.
// |parent| is the parent HWND, created and destroyed on the browser UI thread.
void NotifyPluginProcessHostHelper(HWND window, HWND parent, int tries) {
  // How long to wait between each try.
  static const int kTryDelayMs = 200;

  DWORD plugin_process_id;
  bool found_starting_plugin_process = false;
  GetWindowThreadProcessId(window, &plugin_process_id);
  for (PluginProcessHostIterator iter; !iter.Done(); ++iter) {
    if (!iter.GetData().handle) {
      found_starting_plugin_process = true;
      continue;
    }
    if (base::GetProcId(iter.GetData().handle) == plugin_process_id) {
      iter->AddWindow(parent);
      return;
    }
  }

  if (found_starting_plugin_process) {
    // A plugin process has started but we don't have its handle yet.  Since
    // it's most likely the one for this plugin, try a few more times after a
    // delay.
    if (tries > 0) {
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&NotifyPluginProcessHostHelper, window, parent, tries - 1),
          base::TimeDelta::FromMilliseconds(kTryDelayMs));
      return;
    }
  }

  // The plugin process might have died in the time to execute the task, don't
  // leak the HWND.
  PostMessage(parent, WM_CLOSE, 0, 0);
}

// The plugin wrapper window which lives in the browser process has this proc
// as its window procedure. We only handle the WM_PARENTNOTIFY message sent by
// windowed plugins for mouse input. This is forwarded off to the wrappers
// parent which is typically the RVH window which turns on user gesture.
LRESULT CALLBACK PluginWrapperWindowProc(HWND window, unsigned int message,
                                         WPARAM wparam, LPARAM lparam) {
  if (message == WM_PARENTNOTIFY) {
    switch (LOWORD(wparam)) {
      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_MBUTTONDOWN:
        ::SendMessage(GetParent(window), message, wparam, lparam);
        return 0;
      default:
        break;
    }
  }
  return ::DefWindowProc(window, message, wparam, lparam);
}

// Create an intermediate window between the given HWND and its parent.
HWND ReparentWindow(HWND window) {
  static ATOM atom = 0;
  static HMODULE instance = NULL;
  if (!atom) {
    WNDCLASSEX window_class;
    base::win::InitializeWindowClass(
        webkit::npapi::kWrapperNativeWindowClassName,
        &base::win::WrappedWindowProc<PluginWrapperWindowProc>,
        CS_DBLCLKS,
        0,
        0,
        NULL,
        // xxx reinterpret_cast<HBRUSH>(COLOR_WINDOW+1),
        reinterpret_cast<HBRUSH>(COLOR_GRAYTEXT+1),
        NULL,
        NULL,
        NULL,
        &window_class);
    instance = window_class.hInstance;
    atom = RegisterClassEx(&window_class);
  }
  DCHECK(atom);

  HWND orig_parent = ::GetParent(window);
  HWND parent = CreateWindowEx(
      WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR,
      MAKEINTATOM(atom), 0,
      WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
      0, 0, 0, 0, orig_parent, 0, instance, 0);
  ui::CheckWindowCreated(parent);
  // If UIPI is enabled we need to add message filters for parents with
  // children that cross process boundaries.
  if (::GetPropW(orig_parent, webkit::npapi::kNativeWindowClassFilterProp)) {
    // Process-wide message filters required on Vista must be added to:
    // chrome_content_client.cc ChromeContentClient::SandboxPlugin
    ChangeWindowMessageFilterEx(parent, WM_MOUSEWHEEL, MSGFLT_ALLOW, NULL);
    ChangeWindowMessageFilterEx(parent, WM_GESTURE, MSGFLT_ALLOW, NULL);
    ChangeWindowMessageFilterEx(parent, WM_APPCOMMAND, MSGFLT_ALLOW, NULL);
    ::RemovePropW(orig_parent, webkit::npapi::kNativeWindowClassFilterProp);
  }
  ::SetParent(window, parent);
  // How many times we try to find a PluginProcessHost whose process matches
  // the HWND.
  static const int kMaxTries = 5;
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&NotifyPluginProcessHostHelper, window, parent, kMaxTries));
  return parent;
}

BOOL CALLBACK PainEnumChildProc(HWND hwnd, LPARAM lparam) {
  if (!webkit::npapi::WebPluginDelegateImpl::IsPluginDelegateWindow(hwnd))
    return TRUE;

  gfx::Rect* rect = reinterpret_cast<gfx::Rect*>(lparam);
  static UINT msg = RegisterWindowMessage(webkit::npapi::kPaintMessageName);
  WPARAM wparam = rect->x() << 16 | rect->y();
  lparam = rect->width() << 16 | rect->height();

  // SendMessage gets the message across much quicker than PostMessage, since it
  // doesn't get queued.  When the plugin thread calls PeekMessage or other
  // Win32 APIs, sent messages are dispatched automatically.
  SendNotifyMessage(hwnd, msg, wparam, lparam);

  return TRUE;
}

}  // namespace

// static
void RenderWidgetHostViewBase::MovePluginWindowsHelper(
    HWND parent,
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
  if (moves.empty())
    return;

  bool oop_plugins =
    !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess) &&
    !CommandLine::ForCurrentProcess()->HasSwitch(switches::kInProcessPlugins);

  HDWP defer_window_pos_info =
      ::BeginDeferWindowPos(static_cast<int>(moves.size()));

  if (!defer_window_pos_info) {
    NOTREACHED();
    return;
  }

  for (size_t i = 0; i < moves.size(); ++i) {
    unsigned long flags = 0;
    const webkit::npapi::WebPluginGeometry& move = moves[i];
    HWND window = move.window;

    // As the plugin parent window which lives on the browser UI thread is
    // destroyed asynchronously, it is possible that we have a stale window
    // sent in by the renderer for moving around.
    // Note: get the parent before checking if the window is valid, to avoid a
    // race condition where the window is destroyed after the check but before
    // the GetParent call.
    HWND cur_parent = ::GetParent(window);
    if (!::IsWindow(window))
      continue;

    if (oop_plugins) {
      if (cur_parent == parent) {
        // The plugin window is a direct child of this window, add an
        // intermediate window that lives on this thread to speed up scrolling.
        // Note this only works with out of process plugins since we depend on
        // PluginProcessHost to destroy the intermediate HWNDs.
        cur_parent = ReparentWindow(window);
        ::ShowWindow(window, SW_SHOW);  // Window was created hidden.
      } else if (::GetParent(cur_parent) != parent) {
        // The renderer should only be trying to move windows that are children
        // of its render widget window. However, this may happen as a result of
        // a race condition, so we ignore it and not kill the plugin process.
        continue;
      }

      // We move the intermediate parent window which doesn't result in cross-
      // process synchronous Windows messages.
      window = cur_parent;
    }

    if (move.visible)
      flags |= SWP_SHOWWINDOW;
    else
      flags |= SWP_HIDEWINDOW;

#if defined(USE_AURA)
    // Without this flag, Windows repaints the parent area uncovered by this
    // move. However it only looks at the plugin rectangle and ignores the
    // clipping region. In Aura, the browser chrome could be under the plugin,
    // and if Windows tries to paint it synchronously inside EndDeferWindowsPos
    // then it won't have the data and it will flash white. So instead we
    // manually redraw the plugin.
    // Why not do this for native Windows? Not sure if there are any performance
    // issues with this.
    flags |= SWP_NOREDRAW;
#endif

    if (move.rects_valid) {
      HRGN hrgn = ::CreateRectRgn(move.clip_rect.x(),
                                  move.clip_rect.y(),
                                  move.clip_rect.right(),
                                  move.clip_rect.bottom());
      gfx::SubtractRectanglesFromRegion(hrgn, move.cutout_rects);

      // Note: System will own the hrgn after we call SetWindowRgn,
      // so we don't need to call DeleteObject(hrgn)
      ::SetWindowRgn(window, hrgn, !move.clip_rect.IsEmpty());
    } else {
      flags |= SWP_NOMOVE;
      flags |= SWP_NOSIZE;
    }

    defer_window_pos_info = ::DeferWindowPos(defer_window_pos_info,
                                             window, NULL,
                                             move.window_rect.x(),
                                             move.window_rect.y(),
                                             move.window_rect.width(),
                                             move.window_rect.height(), flags);

    if (!defer_window_pos_info) {
      DCHECK(false) << "DeferWindowPos failed, so all plugin moves ignored.";
      return;
    }
  }

  ::EndDeferWindowPos(defer_window_pos_info);

#if defined(USE_AURA)
  for (size_t i = 0; i < moves.size(); ++i) {
    const webkit::npapi::WebPluginGeometry& move = moves[i];
    RECT r;
    GetWindowRect(move.window, &r);
    gfx::Rect gr(r);
    PainEnumChildProc(move.window, reinterpret_cast<LPARAM>(&gr));
  }
#endif
}

// static
void RenderWidgetHostViewBase::PaintPluginWindowsHelper(
    HWND parent, const gfx::Rect& damaged_screen_rect) {
  LPARAM lparam = reinterpret_cast<LPARAM>(&damaged_screen_rect);
  EnumChildWindows(parent, PainEnumChildProc, lparam);
}

#endif  // OS_WIN

RenderWidgetHostViewBase::RenderWidgetHostViewBase()
    : popup_type_(WebKit::WebPopupTypeNone),
      mouse_locked_(false),
      showing_context_menu_(false),
      selection_text_offset_(0),
      selection_range_(ui::Range::InvalidRange()),
      current_device_scale_factor_(0) {
}

RenderWidgetHostViewBase::~RenderWidgetHostViewBase() {
  DCHECK(!mouse_locked_);
}

void RenderWidgetHostViewBase::SetBackground(const SkBitmap& background) {
  background_ = background;
}

const SkBitmap& RenderWidgetHostViewBase::GetBackground() {
  return background_;
}

void RenderWidgetHostViewBase::SelectionChanged(const string16& text,
                                                size_t offset,
                                                const ui::Range& range) {
  selection_text_ = text;
  selection_text_offset_ = offset;
  selection_range_.set_start(range.start());
  selection_range_.set_end(range.end());
}

bool RenderWidgetHostViewBase::IsShowingContextMenu() const {
  return showing_context_menu_;
}

void RenderWidgetHostViewBase::SetShowingContextMenu(bool showing) {
  DCHECK_NE(showing_context_menu_, showing);
  showing_context_menu_ = showing;
}

bool RenderWidgetHostViewBase::IsMouseLocked() {
  return mouse_locked_;
}

void RenderWidgetHostViewBase::UnhandledWheelEvent(
    const WebKit::WebMouseWheelEvent& event) {
  // Most implementations don't need to do anything here.
}

void RenderWidgetHostViewBase::SetPopupType(WebKit::WebPopupType popup_type) {
  popup_type_ = popup_type;
}

WebKit::WebPopupType RenderWidgetHostViewBase::GetPopupType() {
  return popup_type_;
}

BrowserAccessibilityManager*
    RenderWidgetHostViewBase::GetBrowserAccessibilityManager() const {
  return browser_accessibility_manager_.get();
}

void RenderWidgetHostViewBase::SetBrowserAccessibilityManager(
    BrowserAccessibilityManager* manager) {
  browser_accessibility_manager_.reset(manager);
}

void RenderWidgetHostViewBase::UpdateScreenInfo() {
  gfx::Display display = gfx::Screen::GetDisplayNearestPoint(
      GetViewBounds().origin());
  if (current_display_area_ == display.bounds() &&
      current_device_scale_factor_ == display.device_scale_factor())
    return;
  current_display_area_ = display.bounds();
  current_device_scale_factor_ = display.device_scale_factor();
  if (GetRenderWidgetHost()) {
    RenderWidgetHostImpl* impl =
        RenderWidgetHostImpl::From(GetRenderWidgetHost());
    impl->NotifyScreenInfoChanged();
  }
}

class BasicMouseWheelSmoothScrollGesture
    : public SmoothScrollGesture {
 public:
  BasicMouseWheelSmoothScrollGesture(bool scroll_down, bool scroll_far)
      : start_time_(base::TimeTicks::HighResNow()),
        scroll_down_(scroll_down),
        scroll_far_(scroll_far) { }

  virtual bool ForwardInputEvents(base::TimeTicks now,
                                  RenderWidgetHost* host) OVERRIDE {
    int64 duration_in_ms;
    if (scroll_far_)
      duration_in_ms = kDurationOfFarScrollGestureMs;
    else
      duration_in_ms = kDurationOfNearScrollGestureMs;

    if (now - start_time_ > base::TimeDelta::FromMilliseconds(duration_in_ms))
      return false;

    WebKit::WebMouseWheelEvent event;
    event.type = WebKit::WebInputEvent::MouseWheel;
    // TODO(nduca): Figure out plausible value.
    event.deltaY = scroll_down_ ? -10 : 10;
    event.wheelTicksY = scroll_down_ ? 1 : -1;
    event.modifiers = 0;

    // TODO(nduca): Figure out plausible x and y values.
    event.globalX = 0;
    event.globalY = 0;
    event.x = 0;
    event.y = 0;
    event.windowX = event.x;
    event.windowY = event.y;
    host->ForwardWheelEvent(event);
    return true;
  }

 private:
  virtual ~BasicMouseWheelSmoothScrollGesture() { }
  base::TimeTicks start_time_;
  bool scroll_down_;
  bool scroll_far_;
};

SmoothScrollGesture* RenderWidgetHostViewBase::CreateSmoothScrollGesture(
    bool scroll_down, bool scroll_far) {
  return new BasicMouseWheelSmoothScrollGesture(scroll_down, scroll_far);
}

}  // namespace content
