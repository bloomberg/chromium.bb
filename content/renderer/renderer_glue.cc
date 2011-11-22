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
#include "content/common/npobject_util.h"
#include "content/common/socket_stream_dispatcher.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_thread_impl.h"
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

// This definition of WriteBitmapFromPixels uses shared memory to communicate
// across processes.
void ScopedClipboardWriterGlue::WriteBitmapFromPixels(const void* pixels,
                                                      const gfx::Size& size) {
  // Do not try to write a bitmap more than once
  if (shared_buf_)
    return;

  uint32 buf_size = 4 * size.width() * size.height();

  // Allocate a shared memory buffer to hold the bitmap bits.
  shared_buf_ = ChildThread::current()->AllocateSharedMemory(buf_size);
  if (!shared_buf_)
    return;

  // Copy the bits into shared memory
  DCHECK(shared_buf_->memory());
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
    RenderThreadImpl::current()->Send(
        new ClipboardHostMsg_WriteObjectsSync(objects_,
                shared_buf_->handle()));
    delete shared_buf_;
    return;
  }

  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_WriteObjectsAsync(objects_));
}

namespace webkit_glue {

// Clipboard glue

ui::Clipboard* ClipboardGetClipboard() {
  return NULL;
}

uint64 ClipboardGetSequenceNumber(ui::Clipboard::Buffer buffer) {
  uint64 sequence_number = 0;
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_GetSequenceNumber(buffer,
                                             &sequence_number));
  return sequence_number;
}

bool ClipboardIsFormatAvailable(const ui::Clipboard::FormatType& format,
                                ui::Clipboard::Buffer buffer) {
  bool result;
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_IsFormatAvailable(format, buffer, &result));
  return result;
}

void ClipboardReadAvailableTypes(ui::Clipboard::Buffer buffer,
                                 std::vector<string16>* types,
                                 bool* contains_filenames) {
  RenderThreadImpl::current()->Send(new ClipboardHostMsg_ReadAvailableTypes(
      buffer, types, contains_filenames));
}

void ClipboardReadText(ui::Clipboard::Buffer buffer, string16* result) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_ReadText(buffer, result));
}

void ClipboardReadAsciiText(ui::Clipboard::Buffer buffer, std::string* result) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_ReadAsciiText(buffer, result));
}

void ClipboardReadHTML(ui::Clipboard::Buffer buffer, string16* markup,
                       GURL* url, uint32* fragment_start,
                       uint32* fragment_end) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_ReadHTML(buffer, markup, url, fragment_start,
                                    fragment_end));
}

void ClipboardReadImage(ui::Clipboard::Buffer buffer, std::string* data) {
  base::SharedMemoryHandle image_handle;
  uint32 image_size;
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_ReadImage(buffer, &image_handle, &image_size));
  if (base::SharedMemory::IsHandleValid(image_handle)) {
    base::SharedMemory buffer(image_handle, true);
    buffer.Map(image_size);
    data->append(static_cast<char*>(buffer.memory()), image_size);
  }
}

void GetPlugins(bool refresh,
                std::vector<webkit::WebPluginInfo>* plugins) {
  if (!RenderThreadImpl::current()->plugin_refresh_allowed())
    refresh = false;
  RenderThreadImpl::current()->Send(
      new ViewHostMsg_GetPlugins(refresh, plugins));
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

string16 GetLocalizedString(int message_id) {
  return content::GetContentClient()->GetLocalizedString(message_id);
}

base::StringPiece GetDataResource(int resource_id) {
  return content::GetContentClient()->GetDataResource(resource_id);
}

}  // namespace webkit_glue
