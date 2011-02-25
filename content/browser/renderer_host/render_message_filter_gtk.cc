// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_message_filter.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/common/render_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/x11/WebScreenInfoFactory.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gtk_native_view_id_manager.h"

using WebKit::WebScreenInfo;
using WebKit::WebScreenInfoFactory;

// We get null window_ids passed into the two functions below; please see
// http://crbug.com/9060 for more details.

void RenderMessageFilter::DoOnGetScreenInfo(gfx::NativeViewId view,
                                            IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::BACKGROUND_X11));
  Display* display = ui::GetSecondaryDisplay();
  int screen = ui::GetDefaultScreen(display);
  WebScreenInfo results = WebScreenInfoFactory::screenInfo(display, screen);
  ViewHostMsg_GetScreenInfo::WriteReplyParams(reply_msg, results);
  Send(reply_msg);
}

void RenderMessageFilter::DoOnGetWindowRect(gfx::NativeViewId view,
                                            IPC::Message* reply_msg) {
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

  ViewHostMsg_GetWindowRect::WriteReplyParams(reply_msg, rect);
  Send(reply_msg);
}

// Return the top-level parent of the given window. Called on the
// BACKGROUND_X11 thread.
static XID GetTopLevelWindow(XID window) {
  bool parent_is_root;
  XID parent_window;

  if (!ui::GetWindowParent(&parent_window, &parent_is_root, window))
    return 0;
  if (parent_is_root)
    return window;

  return GetTopLevelWindow(parent_window);
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

void RenderMessageFilter::DoOnClipboardIsFormatAvailable(
    ui::Clipboard::FormatType format, ui::Clipboard::Buffer buffer,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const bool result = GetClipboard()->IsFormatAvailable(format, buffer);

  ViewHostMsg_ClipboardIsFormatAvailable::WriteReplyParams(reply_msg, result);
  Send(reply_msg);
}

void RenderMessageFilter::DoOnClipboardReadText(ui::Clipboard::Buffer buffer,
                                                IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  string16 result;
  GetClipboard()->ReadText(buffer, &result);

  ViewHostMsg_ClipboardReadText::WriteReplyParams(reply_msg, result);
  Send(reply_msg);
}

void RenderMessageFilter::DoOnClipboardReadAsciiText(
    ui::Clipboard::Buffer buffer, IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string result;
  GetClipboard()->ReadAsciiText(buffer, &result);

  ViewHostMsg_ClipboardReadAsciiText::WriteReplyParams(reply_msg, result);
  Send(reply_msg);
}

void RenderMessageFilter::DoOnClipboardReadHTML(ui::Clipboard::Buffer buffer,
                                                IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string src_url_str;
  string16 markup;
  GetClipboard()->ReadHTML(buffer, &markup, &src_url_str);
  const GURL src_url = GURL(src_url_str);

  ViewHostMsg_ClipboardReadHTML::WriteReplyParams(reply_msg, markup, src_url);
  Send(reply_msg);
}

void RenderMessageFilter::DoOnClipboardReadAvailableTypes(
    ui::Clipboard::Buffer buffer, IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Send(reply_msg);
}

void RenderMessageFilter::DoOnClipboardReadData(ui::Clipboard::Buffer buffer,
                                                const string16& type,
                                                IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Send(reply_msg);
}
void RenderMessageFilter::DoOnClipboardReadFilenames(
    ui::Clipboard::Buffer buffer, IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

void RenderMessageFilter::OnClipboardIsFormatAvailable(
    ui::Clipboard::FormatType format, ui::Clipboard::Buffer buffer,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnClipboardIsFormatAvailable, format,
          buffer, reply_msg));
}

void RenderMessageFilter::OnClipboardReadText(ui::Clipboard::Buffer buffer,
                                              IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnClipboardReadText, buffer,
          reply_msg));
}

void RenderMessageFilter::OnClipboardReadAsciiText(ui::Clipboard::Buffer buffer,
                                                   IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnClipboardReadAsciiText, buffer,
          reply_msg));
}

void RenderMessageFilter::OnClipboardReadHTML(ui::Clipboard::Buffer buffer,
                                              IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnClipboardReadHTML, buffer,
          reply_msg));
}

void RenderMessageFilter::OnClipboardReadAvailableTypes(
    ui::Clipboard::Buffer buffer, IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnClipboardReadAvailableTypes, buffer,
          reply_msg));
}

void RenderMessageFilter::OnClipboardReadData(ui::Clipboard::Buffer buffer,
                                              const string16& type,
                                              IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnClipboardReadData, buffer, type,
          reply_msg));
}

void RenderMessageFilter::OnClipboardReadFilenames(
    ui::Clipboard::Buffer buffer, IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnClipboardReadFilenames, buffer,
          reply_msg));
}
