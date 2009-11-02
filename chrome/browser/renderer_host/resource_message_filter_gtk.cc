// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/resource_message_filter.h"

#include <fcntl.h>
#include <map>

#include "app/clipboard/clipboard.h"
#include "app/gfx/gtk_native_view_id_manager.h"
#include "app/l10n_util.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "chrome/browser/chrome_thread.h"
#if defined(TOOLKIT_GTK)
#include "chrome/browser/printing/print_dialog_gtk.h"
#endif
#include "chrome/common/chrome_paths.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/x11_util.h"
#include "grit/generated_resources.h"

#include "webkit/api/public/WebScreenInfo.h"
#include "webkit/api/public/x11/WebScreenInfoFactory.h"

using WebKit::WebScreenInfo;
using WebKit::WebScreenInfoFactory;

namespace {

typedef std::map<int, FilePath> FdMap;

struct PrintingFileDescriptorMap {
  FdMap map;
};

}  // namespace

// We get null window_ids passed into the two functions below; please see
// http://crbug.com/9060 for more details.

// Called on the IO thread.
void ResourceMessageFilter::SendDelayedReply(IPC::Message* reply_msg) {
  Send(reply_msg);
}

// Called on the BACKGROUND_X11 thread.
void ResourceMessageFilter::DoOnGetScreenInfo(gfx::NativeViewId view,
                                              IPC::Message* reply_msg) {
  Display* display = x11_util::GetSecondaryDisplay();
  int screen = x11_util::GetDefaultScreen(display);
  WebScreenInfo results = WebScreenInfoFactory::screenInfo(display, screen);
  ViewHostMsg_GetScreenInfo::WriteReplyParams(reply_msg, results);

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Called on the BACKGROUND_X11 thread.
void ResourceMessageFilter::DoOnGetWindowRect(gfx::NativeViewId view,
                                              IPC::Message* reply_msg) {
  // This is called to get the x, y offset (in screen coordinates) of the given
  // view and its width and height.
  gfx::Rect rect;
  XID window;

  if (Singleton<GtkNativeViewManager>()->GetXIDForId(&window, view)) {
    if (window) {
      int x, y;
      unsigned width, height;
      if (x11_util::GetWindowGeometry(&x, &y, &width, &height, window))
        rect = gfx::Rect(x, y, width, height);
    }
  }

  ViewHostMsg_GetWindowRect::WriteReplyParams(reply_msg, rect);

   ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Return the top-level parent of the given window. Called on the
// BACKGROUND_X11 thread.
static XID GetTopLevelWindow(XID window) {
  bool parent_is_root;
  XID parent_window;

  if (!x11_util::GetWindowParent(&parent_window, &parent_is_root, window))
    return 0;
  if (parent_is_root)
    return window;

  return GetTopLevelWindow(parent_window);
}

// Called on the BACKGROUND_X11 thread.
void ResourceMessageFilter::DoOnGetRootWindowRect(gfx::NativeViewId view,
                                                  IPC::Message* reply_msg) {
  // This is called to get the screen coordinates and size of the browser
  // window itself.
  gfx::Rect rect;
  XID window;

  if (Singleton<GtkNativeViewManager>()->GetXIDForId(&window, view)) {
    if (window) {
      const XID toplevel = GetTopLevelWindow(window);
      int x, y;
      unsigned width, height;
      if (x11_util::GetWindowGeometry(&x, &y, &width, &height, toplevel))
        rect = gfx::Rect(x, y, width, height);
    }
  }

  ViewHostMsg_GetRootWindowRect::WriteReplyParams(reply_msg, rect);

   ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Called on the UI thread.
void ResourceMessageFilter::DoOnClipboardIsFormatAvailable(
    Clipboard::FormatType format, Clipboard::Buffer buffer,
    IPC::Message* reply_msg) {
  const bool result = GetClipboard()->IsFormatAvailable(format, buffer);

  ViewHostMsg_ClipboardIsFormatAvailable::WriteReplyParams(reply_msg, result);

   ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Called on the UI thread.
void ResourceMessageFilter::DoOnClipboardReadText(Clipboard::Buffer buffer,
                                                  IPC::Message* reply_msg) {
  string16 result;
  GetClipboard()->ReadText(buffer, &result);

  ViewHostMsg_ClipboardReadText::WriteReplyParams(reply_msg, result);

   ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Called on the UI thread.
void ResourceMessageFilter::DoOnClipboardReadAsciiText(
    Clipboard::Buffer buffer, IPC::Message* reply_msg) {
  std::string result;
  GetClipboard()->ReadAsciiText(buffer, &result);

  ViewHostMsg_ClipboardReadAsciiText::WriteReplyParams(reply_msg, result);

   ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Called on the UI thread.
void ResourceMessageFilter::DoOnClipboardReadHTML(Clipboard::Buffer buffer,
                                                  IPC::Message* reply_msg) {
  std::string src_url_str;
  string16 markup;
  GetClipboard()->ReadHTML(buffer, &markup, &src_url_str);
  const GURL src_url = GURL(src_url_str);

  ViewHostMsg_ClipboardReadHTML::WriteReplyParams(reply_msg, markup, src_url);

   ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Called on the FILE thread.
void ResourceMessageFilter::DoOnAllocateTempFileForPrinting(
    IPC::Message* reply_msg) {
  base::FileDescriptor temp_file_fd;
  int fd_in_browser;
  temp_file_fd.fd = fd_in_browser = -1;
  temp_file_fd.auto_close = false;

  FilePath path;
  if (file_util::CreateTemporaryFile(&path)) {
    int fd = open(path.value().c_str(), O_WRONLY);
    if (fd >= 0) {
      FdMap* map = &Singleton<PrintingFileDescriptorMap>::get()->map;
      FdMap::iterator it = map->find(fd);
      if (it != map->end()) {
        NOTREACHED() << "The file descriptor is in use. fd=" << fd;
      } else {
        (*map)[fd] = path;
        temp_file_fd.fd = fd_in_browser = fd;
        temp_file_fd.auto_close = true;
      }
    }
  }

  ViewHostMsg_AllocateTempFileForPrinting::WriteReplyParams(
      reply_msg, temp_file_fd, fd_in_browser);

   ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnGetScreenInfo(gfx::NativeViewId view,
                                            IPC::Message* reply_msg) {
   ChromeThread::PostTask(
      ChromeThread::BACKGROUND_X11, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::DoOnGetScreenInfo, view, reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnGetWindowRect(gfx::NativeViewId view,
                                            IPC::Message* reply_msg) {
   ChromeThread::PostTask(
      ChromeThread::BACKGROUND_X11, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::DoOnGetWindowRect, view, reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnGetRootWindowRect(gfx::NativeViewId view,
                                                IPC::Message* reply_msg) {
   ChromeThread::PostTask(
      ChromeThread::BACKGROUND_X11, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::DoOnGetRootWindowRect, view,
          reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnClipboardIsFormatAvailable(
    Clipboard::FormatType format, Clipboard::Buffer buffer,
    IPC::Message* reply_msg) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::DoOnClipboardIsFormatAvailable, format,
          buffer, reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnClipboardReadText(Clipboard::Buffer buffer,
                                                IPC::Message* reply_msg) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::DoOnClipboardReadText, buffer,
          reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnClipboardReadAsciiText(Clipboard::Buffer buffer,
                                                     IPC::Message* reply_msg) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::DoOnClipboardReadAsciiText, buffer,
          reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnClipboardReadHTML(Clipboard::Buffer buffer,
                                                IPC::Message* reply_msg) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::DoOnClipboardReadHTML, buffer,
          reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnAllocateTempFileForPrinting(
    IPC::Message* reply_msg) {
   ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::DoOnAllocateTempFileForPrinting,
          reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnTempFileForPrintingWritten(int fd_in_browser) {
  FdMap* map = &Singleton<PrintingFileDescriptorMap>::get()->map;
  FdMap::iterator it = map->find(fd_in_browser);
  if (it == map->end()) {
    NOTREACHED() << "Got a file descriptor that we didn't pass to the "
                    "renderer: " << fd_in_browser;
    return;
  }

#if defined(TOOLKIT_GTK)
  PrintDialogGtk::CreatePrintDialogForPdf(it->second);
#else
  NOTIMPLEMENTED();
#endif

  // Erase the entry in the map.
  map->erase(it);
}
