// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides the embedder's side of random webkit glue functions.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/shared_memory.h"
#include "base/string_util.h"
#include "content/common/clipboard_messages.h"
#include "content/common/content_client.h"
#include "content/common/content_switches.h"
#include "content/common/npobject_util.h"
#include "content/common/socket_stream_dispatcher.h"
#include "content/common/url_constants.h"
#include "content/common/view_messages.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_thread.h"
#include "googleurl/src/url_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKitPlatformSupport.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ui_base_switches.h"
#include "webkit/glue/scoped_clipboard_writer_glue.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/websocketstreamhandle_bridge.h"

#if defined(OS_LINUX)
#include "content/common/child_process_sandbox_support_linux.h"
#endif

// This definition of WriteBitmapFromPixels uses shared memory to communicate
// across processes.
void ScopedClipboardWriterGlue::WriteBitmapFromPixels(const void* pixels,
                                                      const gfx::Size& size) {
  // Do not try to write a bitmap more than once
  if (shared_buf_)
    return;

  uint32 buf_size = 4 * size.width() * size.height();

  // Allocate a shared memory buffer to hold the bitmap bits.
#if defined(OS_POSIX)
  // On POSIX, we need to ask the browser to create the shared memory for us,
  // since this is blocked by the sandbox.
  base::SharedMemoryHandle shared_mem_handle;
  ViewHostMsg_AllocateSharedMemoryBuffer *msg =
      new ViewHostMsg_AllocateSharedMemoryBuffer(buf_size,
                                                 &shared_mem_handle);
  if (RenderThread::current()->Send(msg)) {
    if (base::SharedMemory::IsHandleValid(shared_mem_handle)) {
      shared_buf_ = new base::SharedMemory(shared_mem_handle, false);
      if (!shared_buf_ || !shared_buf_->Map(buf_size)) {
        NOTREACHED() << "Map failed";
        return;
      }
    } else {
      NOTREACHED() << "Browser failed to allocate shared memory";
      return;
    }
  } else {
    NOTREACHED() << "Browser allocation request message failed";
    return;
  }
#else  // !OS_POSIX
  shared_buf_ = new base::SharedMemory;
  if (!shared_buf_->CreateAndMapAnonymous(buf_size)) {
    NOTREACHED();
    return;
  }
#endif

  // Copy the bits into shared memory
  memcpy(shared_buf_->memory(), pixels, buf_size);
  shared_buf_->Unmap();

  ui::Clipboard::ObjectMapParam size_param;
  const char* size_data = reinterpret_cast<const char*>(&size);
  for (size_t i = 0; i < sizeof(gfx::Size); ++i)
    size_param.push_back(size_data[i]);

  ui::Clipboard::ObjectMapParams params;

  // The first parameter is replaced on the receiving end with a pointer to
  // a shared memory object containing the bitmap. We reserve space for it here.
  ui::Clipboard::ObjectMapParam place_holder_param;
  params.push_back(place_holder_param);
  params.push_back(size_param);
  objects_[ui::Clipboard::CBF_SMBITMAP] = params;
}

// Define a destructor that makes IPCs to flush the contents to the
// system clipboard.
ScopedClipboardWriterGlue::~ScopedClipboardWriterGlue() {
  if (objects_.empty())
    return;

  if (shared_buf_) {
    RenderThread::current()->Send(
        new ClipboardHostMsg_WriteObjectsSync(objects_,
                shared_buf_->handle()));
    delete shared_buf_;
    return;
  }

  RenderThread::current()->Send(
      new ClipboardHostMsg_WriteObjectsAsync(objects_));
}

namespace webkit_glue {

// Clipboard glue

ui::Clipboard* ClipboardGetClipboard() {
  return NULL;
}

bool ClipboardIsFormatAvailable(const ui::Clipboard::FormatType& format,
                                ui::Clipboard::Buffer buffer) {
  bool result;
  RenderThread::current()->Send(
      new ClipboardHostMsg_IsFormatAvailable(format, buffer, &result));
  return result;
}

void ClipboardReadAvailableTypes(ui::Clipboard::Buffer buffer,
                                 std::vector<string16>* types,
                                 bool* contains_filenames) {
  RenderThread::current()->Send(new ClipboardHostMsg_ReadAvailableTypes(
      buffer, types, contains_filenames));
}

void ClipboardReadText(ui::Clipboard::Buffer buffer, string16* result) {
  RenderThread::current()->Send(new ClipboardHostMsg_ReadText(buffer, result));
}

void ClipboardReadAsciiText(ui::Clipboard::Buffer buffer, std::string* result) {
  RenderThread::current()->Send(
      new ClipboardHostMsg_ReadAsciiText(buffer, result));
}

void ClipboardReadHTML(ui::Clipboard::Buffer buffer, string16* markup,
                       GURL* url) {
  RenderThread::current()->Send(
      new ClipboardHostMsg_ReadHTML(buffer, markup, url));
}

void ClipboardReadImage(ui::Clipboard::Buffer buffer, std::string* data) {
  base::SharedMemoryHandle image_handle;
  uint32 image_size;
  RenderThread::current()->Send(
      new ClipboardHostMsg_ReadImage(buffer, &image_handle, &image_size));
  if (base::SharedMemory::IsHandleValid(image_handle)) {
    base::SharedMemory buffer(image_handle, true);
    buffer.Map(image_size);
    data->append(static_cast<char*>(buffer.memory()), image_size);
  }
}

bool ClipboardReadData(ui::Clipboard::Buffer buffer, const string16& type,
                       string16* data, string16* metadata) {
  bool result = false;
  RenderThread::current()->Send(new ClipboardHostMsg_ReadData(
      buffer, type, &result, data, metadata));
  return result;
}

bool ClipboardReadFilenames(ui::Clipboard::Buffer buffer,
                            std::vector<string16>* filenames) {
  bool result;
  RenderThread::current()->Send(new ClipboardHostMsg_ReadFilenames(
      buffer, &result, filenames));
  return result;
}

uint64 ClipboardGetSequenceNumber() {
  uint64 seq_num = 0;
  RenderThread::current()->Send(
      new ClipboardHostMsg_GetSequenceNumber(&seq_num));
  return seq_num;
}

void GetPlugins(bool refresh,
                std::vector<webkit::WebPluginInfo>* plugins) {
  if (!RenderThread::current()->plugin_refresh_allowed())
    refresh = false;
  RenderThread::current()->Send(new ViewHostMsg_GetPlugins(refresh, plugins));
}

bool IsProtocolSupportedForMedia(const GURL& url) {
  // If new protocol is to be added here, we need to make sure the response is
  // validated accordingly in the media engine.
  if (url.SchemeIsFile() || url.SchemeIs(chrome::kHttpScheme) ||
      url.SchemeIs(chrome::kHttpsScheme) ||
      url.SchemeIs(chrome::kDataScheme) ||
      url.SchemeIs(chrome::kFileSystemScheme) ||
      url.SchemeIs(chrome::kBlobScheme))
    return true;
  return
      content::GetContentClient()->renderer()->IsProtocolSupportedForMedia(url);
}

// static factory function
ResourceLoaderBridge* ResourceLoaderBridge::Create(
    const ResourceLoaderBridge::RequestInfo& request_info) {
  return ChildThread::current()->CreateBridge(request_info);
}

// static factory function
WebSocketStreamHandleBridge* WebSocketStreamHandleBridge::Create(
    WebKit::WebSocketStreamHandle* handle,
    WebSocketStreamHandleDelegate* delegate) {
  SocketStreamDispatcher* dispatcher =
      ChildThread::current()->socket_stream_dispatcher();
  return dispatcher->CreateBridge(handle, delegate);
}

#if defined(OS_LINUX)
int MatchFontWithFallback(const std::string& face, bool bold,
                          bool italic, int charset) {
  return child_process_sandbox_support::MatchFontWithFallback(
      face, bold, italic, charset);
}

bool GetFontTable(int fd, uint32_t table, uint8_t* output,
                  size_t* output_length) {
  return child_process_sandbox_support::GetFontTable(
      fd, table, output, output_length);
}
#endif

string16 GetLocalizedString(int message_id) {
  return content::GetContentClient()->GetLocalizedString(message_id);
}

base::StringPiece GetDataResource(int resource_id) {
  return content::GetContentClient()->GetDataResource(resource_id);
}

}  // namespace webkit_glue
