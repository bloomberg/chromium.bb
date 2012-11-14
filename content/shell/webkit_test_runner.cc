// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/webkit_test_runner.h"

#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "content/public/renderer/render_view.h"
#include "content/shell/shell_messages.h"
#include "content/shell/shell_render_process_observer.h"
#include "net/base/net_util.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebKitPlatformSupport.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebTask.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebContextMenuData;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebGamepads;
using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebVector;
using WebKit::WebView;
using WebTestRunner::WebTask;

namespace content {

namespace {

void InvokeTaskHelper(void* context) {
  WebTask* task = reinterpret_cast<WebTask*>(context);
  task->run();
  delete task;
}

std::string DumpDocumentText(WebFrame* frame) {
  // We use the document element's text instead of the body text here because
  // not all documents have a body, such as XML documents.
  WebElement documentElement = frame->document().documentElement();
  if (documentElement.isNull())
    return std::string();
  return documentElement.innerText().utf8();
}

std::string DumpDocumentPrintedText(WebFrame* frame) {
  return frame->renderTreeAsText(WebFrame::RenderAsTextPrinting).utf8();
}

std::string DumpFramesAsText(WebFrame* frame, bool printing, bool recursive) {
  std::string result;

  // Cannot do printed format for anything other than HTML.
  if (printing && !frame->document().isHTMLDocument())
    return std::string();

  // Add header for all but the main frame. Skip emtpy frames.
  if (frame->parent() && !frame->document().documentElement().isNull()) {
    result.append("\n--------\nFrame: '");
    result.append(frame->uniqueName().utf8().data());
    result.append("'\n--------\n");
  }

  result.append(
      printing ? DumpDocumentPrintedText(frame) : DumpDocumentText(frame));
  result.append("\n");

  if (recursive) {
    for (WebFrame* child = frame->firstChild(); child;
         child = child->nextSibling()) {
      result.append(DumpFramesAsText(child, printing, recursive));
    }
  }
  return result;
}

std::string DumpFrameScrollPosition(WebFrame* frame, bool recursive) {
  std::string result;

  WebSize offset = frame->scrollOffset();
  if (offset.width > 0 || offset.height > 0) {
    if (frame->parent()) {
      result.append(
          base::StringPrintf("frame '%s' ", frame->uniqueName().utf8().data()));
    }
    result.append(
        base::StringPrintf("scrolled to %d,%d\n", offset.width, offset.height));
  }

  if (recursive) {
    for (WebFrame* child = frame->firstChild(); child;
         child = child->nextSibling()) {
      result.append(DumpFrameScrollPosition(child, recursive));
    }
  }
  return result;
}

SkCanvas* PaintViewIntoCanvas(WebView* view) {
  view->layout();
  const WebSize& size = view->size();

  SkCanvas* canvas = skia::CreatePlatformCanvas(size.width, size.height, true,
                                              0, skia::RETURN_NULL_ON_FAILURE);
  if (canvas) {
    view->paint(webkit_glue::ToWebCanvas(canvas),
                WebRect(0, 0, size.width, size.height));
  }
  return canvas;
}

#if !defined(OS_MACOSX)
void MakeBitmapOpaque(SkBitmap* bitmap) {
  SkAutoLockPixels lock(*bitmap);
  DCHECK(bitmap->config() == SkBitmap::kARGB_8888_Config);
  for (int y = 0; y < bitmap->height(); ++y) {
    uint32_t* row = bitmap->getAddr32(0, y);
    for (int x = 0; x < bitmap->width(); ++x)
      row[x] |= 0xFF000000;  // Set alpha bits to 1.
  }
}
#endif

void CaptureSnapshot(WebView* view, SkBitmap* snapshot) {
  SkCanvas* canvas = PaintViewIntoCanvas(view);
  if (!canvas)
    return;

  SkDevice* device = skia::GetTopDevice(*canvas);

  const SkBitmap& bitmap = device->accessBitmap(false);
  bitmap.copyTo(snapshot, SkBitmap::kARGB_8888_Config);

#if !defined(OS_MACOSX)
  // Only the expected PNGs for Mac have a valid alpha channel.
  MakeBitmapOpaque(snapshot);
#endif

}

}  // namespace

WebKitTestRunner::WebKitTestRunner(RenderView* render_view)
    : RenderViewObserver(render_view),
      is_main_window_(false) {
}

WebKitTestRunner::~WebKitTestRunner() {
  if (is_main_window_)
    ShellRenderProcessObserver::GetInstance()->SetMainWindow(NULL, this);
}

// WebTestDelegate  -----------------------------------------------------------

void WebKitTestRunner::clearContextMenuData() {
  last_context_menu_data_.reset();
}

WebContextMenuData* WebKitTestRunner::lastContextMenuData() const {
  return last_context_menu_data_.get();
}

void WebKitTestRunner::clearEditCommand() {
  render_view()->ClearEditCommands();
}

void WebKitTestRunner::setEditCommand(const std::string& name,
                                      const std::string& value) {
  render_view()->SetEditCommandForNextKeyEvent(name, value);
}

void WebKitTestRunner::fillSpellingSuggestionList(
    const WebString& word, WebVector<WebString>* suggestions) {
  if (word == WebString::fromUTF8("wellcome")) {
      WebVector<WebString> result(suggestions->size() + 1);
      for (size_t i = 0; i < suggestions->size(); ++i)
        result[i] = (*suggestions)[i];
      result[suggestions->size()] = WebString::fromUTF8("welcome");
      suggestions->swap(result);
  }
}

void WebKitTestRunner::setGamepadData(const WebGamepads& gamepads) {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebTestDelegate", "setGamepadData"));
}

void WebKitTestRunner::printMessage(const std::string& message) {
  Send(new ShellViewHostMsg_PrintMessage(routing_id(), message));
}

void WebKitTestRunner::postTask(WebTask* task) {
  WebKit::webKitPlatformSupport()->callOnMainThread(InvokeTaskHelper, task);
}

void WebKitTestRunner::postDelayedTask(WebTask* task, long long ms) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WebTask::run, base::Owned(task)),
      base::TimeDelta::FromMilliseconds(ms));
}

WebString WebKitTestRunner::registerIsolatedFileSystem(
    const WebKit::WebVector<WebKit::WebString>& absolute_filenames) {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebTestDelegate", "registerIsolatedFileSystem"));
  return WebString();
}

long long WebKitTestRunner::getCurrentTimeInMillisecond() {
    return base::TimeTicks::Now().ToInternalValue() /
        base::Time::kMicrosecondsPerMillisecond;
}

WebString WebKitTestRunner::getAbsoluteWebStringFromUTF8Path(
    const std::string& utf8_path) {
#if defined(OS_WIN)
  FilePath path(UTF8ToWide(utf8_path));
#else
  FilePath path(base::SysWideToNativeMB(base::SysUTF8ToWide(utf8_path)));
#endif
  if (!path.IsAbsolute()) {
    GURL base_url =
        net::FilePathToFileURL(current_working_directory_.Append(
            FILE_PATH_LITERAL("foo")));
    net::FileURLToFilePath(base_url.Resolve(utf8_path), &path);
  }
#if defined(OS_WIN)
  return WebString(path.value());
#else
  return WideToUTF16(base::SysNativeMBToWide(path.value()));
#endif
}

// RenderViewObserver  --------------------------------------------------------

void WebKitTestRunner::DidClearWindowObject(WebFrame* frame) {
  ShellRenderProcessObserver::GetInstance()->BindTestRunnersToWindow(frame);
}

void WebKitTestRunner::DidFinishLoad(WebFrame* frame) {
  if (!frame->parent())
    Send(new ShellViewHostMsg_DidFinishLoad(routing_id()));
}

void WebKitTestRunner::DidRequestShowContextMenu(
    WebFrame* frame,
    const WebContextMenuData& data) {
  last_context_menu_data_.reset(new WebContextMenuData(data));
}

bool WebKitTestRunner::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebKitTestRunner, message)
    IPC_MESSAGE_HANDLER(ShellViewMsg_CaptureTextDump, OnCaptureTextDump)
    IPC_MESSAGE_HANDLER(ShellViewMsg_CaptureImageDump, OnCaptureImageDump)
    IPC_MESSAGE_HANDLER(ShellViewMsg_SetCurrentWorkingDirectory,
                        OnSetCurrentWorkingDirectory)
    IPC_MESSAGE_HANDLER(ShellViewMsg_SetIsMainWindow, OnSetIsMainWindow)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

// Private methods  -----------------------------------------------------------

void WebKitTestRunner::OnCaptureTextDump(bool as_text,
                                         bool printing,
                                         bool recursive) {
  WebFrame* frame = render_view()->GetWebView()->mainFrame();
  std::string dump;
  if (as_text) {
    dump = DumpFramesAsText(frame, printing, recursive);
  } else {
    WebFrame::RenderAsTextControls render_text_behavior =
        WebFrame::RenderAsTextNormal;
    if (printing)
      render_text_behavior |= WebFrame::RenderAsTextPrinting;
    dump = frame->renderTreeAsText(render_text_behavior).utf8();
    dump.append(DumpFrameScrollPosition(frame, recursive));
  }
  Send(new ShellViewHostMsg_TextDump(routing_id(), dump));
}

void WebKitTestRunner::OnCaptureImageDump(
    const std::string& expected_pixel_hash) {
  SkBitmap snapshot;
  CaptureSnapshot(render_view()->GetWebView(), &snapshot);

  SkAutoLockPixels snapshot_lock(snapshot);
  base::MD5Digest digest;
#if defined(OS_ANDROID)
  // On Android, pixel layout is RGBA, however, other Chrome platforms use BGRA.
  const uint8_t* raw_pixels =
      reinterpret_cast<const uint8_t*>(snapshot.getPixels());
  size_t snapshot_size = snapshot.getSize();
  scoped_array<uint8_t> reordered_pixels(new uint8_t[snapshot_size]);
  for (size_t i = 0; i < snapshot_size; i += 4) {
    reordered_pixels[i] = raw_pixels[i + 2];
    reordered_pixels[i + 1] = raw_pixels[i + 1];
    reordered_pixels[i + 2] = raw_pixels[i];
    reordered_pixels[i + 3] = raw_pixels[i + 3];
  }
  base::MD5Sum(reordered_pixels.get(), snapshot_size, &digest);
#else
  base::MD5Sum(snapshot.getPixels(), snapshot.getSize(), &digest);
#endif
  std::string actual_pixel_hash = base::MD5DigestToBase16(digest);

  if (actual_pixel_hash == expected_pixel_hash) {
    SkBitmap empty_image;
    Send(new ShellViewHostMsg_ImageDump(
        routing_id(), actual_pixel_hash, empty_image));
    return;
  }
  Send(new ShellViewHostMsg_ImageDump(
      routing_id(), actual_pixel_hash, snapshot));
}

void WebKitTestRunner::OnSetCurrentWorkingDirectory(
    const FilePath& current_working_directory) {
  current_working_directory_ = current_working_directory;
}

void WebKitTestRunner::OnSetIsMainWindow() {
  is_main_window_ = true;
  ShellRenderProcessObserver::GetInstance()->SetMainWindow(render_view(), this);
}

}  // namespace content
