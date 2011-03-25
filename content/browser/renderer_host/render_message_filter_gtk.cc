// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>

#include "content/browser/renderer_host/render_message_filter.h"

#include "content/browser/browser_thread.h"
#include "content/common/view_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/x11/WebScreenInfoFactory.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gtk_native_view_id_manager.h"

#if !defined(OS_CHROMEOS)
// We want to obey the alphabetical order for including header files, but
// #define macros in X11 header files affect chrome header files
// (e.g. Success), so include Xinerama.h here. We don't include Xinerama.h on
// Chrome OS since the X server for Chrome OS does not support the extension.
#include <X11/extensions/Xinerama.h>
#endif  // OS_CHROMEOS

using WebKit::WebScreenInfo;
using WebKit::WebScreenInfoFactory;

namespace {

// Return the top-level parent of the given window. Called on the
// BACKGROUND_X11 thread.
XID GetTopLevelWindow(XID window) {
  bool parent_is_root;
  XID parent_window;

  if (!ui::GetWindowParent(&parent_window, &parent_is_root, window))
    return 0;
  if (parent_is_root)
    return window;

  return GetTopLevelWindow(parent_window);
}

gfx::Rect GetWindowRectFromView(gfx::NativeViewId view) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::BACKGROUND_X11));
  // This is called to get the x, y offset (in screen coordinates) of the given
  // view and its width and height.
  gfx::Rect rect;
  XID window;

  base::AutoLock lock(GtkNativeViewManager::GetInstance()->unrealize_lock());
  if (GtkNativeViewManager::GetInstance()->GetXIDForId(&window, view)) {
    if (window) {
      int x, y;
      unsigned width, height;
      if (ui::GetWindowGeometry(&x, &y, &width, &height, window))
        rect = gfx::Rect(x, y, width, height);
    }
  }
  return rect;
}

}  // namespace

// We get null window_ids passed into the two functions below; please see
// http://crbug.com/9060 for more details.

void RenderMessageFilter::DoOnGetScreenInfo(gfx::NativeViewId view,
                                            IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::BACKGROUND_X11));
  Display* display = ui::GetSecondaryDisplay();

  WebScreenInfo results = WebScreenInfoFactory::screenInfo(display,
      ui::GetDefaultScreen(display));

#if !defined(OS_CHROMEOS)
  // First check if we can use Xinerama.
  XineramaScreenInfo* screen_info = NULL;
  int screen_num = 0;
  void* xinerama_lib = dlopen("libXinerama.so.1", RTLD_LAZY);
  if (xinerama_lib) {
    // Prototypes for Xinerama functions.
    typedef Bool (*XineramaIsActiveFunction)(Display* display);
    typedef XineramaScreenInfo*
        (*XineramaQueryScreensFunction)(Display* display, int* num);

    XineramaIsActiveFunction is_active = (XineramaIsActiveFunction)
        dlsym(xinerama_lib, "XineramaIsActive");

    XineramaQueryScreensFunction query_screens = (XineramaQueryScreensFunction)
        dlsym(xinerama_lib, "XineramaQueryScreens");

    // Get the number of screens via Xinerama
    if (is_active && query_screens && is_active(display))
      screen_info = query_screens(display, &screen_num);

    if (screen_info) {
      // We can use Xinerama information. Calculate the intersect rect size
      // between window rect and screen rect, and choose the biggest size.
      gfx::Rect rect = GetWindowRectFromView(view);
      int max_intersect_index = 0;
      for (int i = 0, max_intersect_size = 0; i < screen_num; ++i) {
        gfx::Rect screen_rect(screen_info[i].x_org, screen_info[i].y_org,
                              screen_info[i].width, screen_info[i].height);
        gfx::Rect intersect_rect = screen_rect.Intersect(rect);
        int intersect_size = intersect_rect.width() * intersect_rect.height();
        if (intersect_size > max_intersect_size) {
          max_intersect_size = intersect_size;
          max_intersect_index = i;
        }
      }
      XineramaScreenInfo& target_screen = screen_info[max_intersect_index];
      results.rect = WebKit::WebRect(target_screen.x_org,
                                     target_screen.y_org,
                                     target_screen.width,
                                     target_screen.height);
      results.availableRect = results.rect;

      XFree(screen_info);
    }
  }
#endif  // OS_CHROMEOS

  ViewHostMsg_GetScreenInfo::WriteReplyParams(reply_msg, results);
  Send(reply_msg);
}

void RenderMessageFilter::DoOnGetWindowRect(gfx::NativeViewId view,
                                            IPC::Message* reply_msg) {
  ViewHostMsg_GetWindowRect::WriteReplyParams(reply_msg,
                                              GetWindowRectFromView(view));
  Send(reply_msg);
}

void RenderMessageFilter::DoOnGetRootWindowRect(gfx::NativeViewId view,
                                                IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::BACKGROUND_X11));
  // This is called to get the screen coordinates and size of the browser
  // window itself.
  gfx::Rect rect;
  XID window;

  base::AutoLock lock(GtkNativeViewManager::GetInstance()->unrealize_lock());
  if (GtkNativeViewManager::GetInstance()->GetXIDForId(&window, view)) {
    if (window) {
      const XID toplevel = GetTopLevelWindow(window);
      if (toplevel) {
        int x, y;
        unsigned width, height;
        if (ui::GetWindowGeometry(&x, &y, &width, &height, toplevel))
          rect = gfx::Rect(x, y, width, height);
      }
    }
  }

  ViewHostMsg_GetRootWindowRect::WriteReplyParams(reply_msg, rect);
  Send(reply_msg);
}

void RenderMessageFilter::OnGetScreenInfo(gfx::NativeViewId view,
                                          IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
     BrowserThread::BACKGROUND_X11, FROM_HERE,
     NewRunnableMethod(
         this, &RenderMessageFilter::DoOnGetScreenInfo, view, reply_msg));
}

void RenderMessageFilter::OnGetWindowRect(gfx::NativeViewId view,
                                          IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::BACKGROUND_X11, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnGetWindowRect, view, reply_msg));
}

void RenderMessageFilter::OnGetRootWindowRect(gfx::NativeViewId view,
                                              IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::BACKGROUND_X11, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnGetRootWindowRect, view, reply_msg));
}
