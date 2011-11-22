// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides the embedder's side of random webkit glue functions.

#include "content/renderer/renderer_clipboard_client.h"

#include "build/build_config.h"

#include <string>
#include <vector>

#include "base/shared_memory.h"
#include "base/string16.h"
#include "content/common/clipboard_messages.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_thread_impl.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/gfx/size.h"
#include "webkit/glue/scoped_clipboard_writer_glue.h"

namespace {

class RendererClipboardWriteContext :
    public webkit_glue::ClipboardClient::WriteContext {
 public:
  RendererClipboardWriteContext();
  virtual ~RendererClipboardWriteContext();
  virtual void WriteBitmapFromPixels(ui::Clipboard::ObjectMap* objects,
                                     const void* pixels,
                                     const gfx::Size& size);
  virtual void FlushAndDestroy(const ui::Clipboard::ObjectMap& objects);

 private:
  scoped_ptr<base::SharedMemory> shared_buf_;
  DISALLOW_COPY_AND_ASSIGN(RendererClipboardWriteContext);
};

RendererClipboardWriteContext::RendererClipboardWriteContext() {
}

RendererClipboardWriteContext::~RendererClipboardWriteContext() {
}

// This definition of WriteBitmapFromPixels uses shared memory to communicate
// across processes.
void RendererClipboardWriteContext::WriteBitmapFromPixels(
    ui::Clipboard::ObjectMap* objects,
    const void* pixels,
    const gfx::Size& size) {
  // Do not try to write a bitmap more than once
  if (shared_buf_.get())
    return;

  uint32 buf_size = 4 * size.width() * size.height();

  // Allocate a shared memory buffer to hold the bitmap bits.
  shared_buf_.reset(ChildThread::current()->AllocateSharedMemory(buf_size));
  if (!shared_buf_.get())
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
  (*objects)[ui::Clipboard::CBF_SMBITMAP] = params;
}

// Define a destructor that makes IPCs to flush the contents to the
// system clipboard.
void RendererClipboardWriteContext::FlushAndDestroy(
    const ui::Clipboard::ObjectMap& objects) {
  scoped_ptr<RendererClipboardWriteContext> delete_on_return(this);
  if (objects.empty())
    return;

  if (shared_buf_.get()) {
    RenderThreadImpl::current()->Send(
        new ClipboardHostMsg_WriteObjectsSync(objects, shared_buf_->handle()));
  } else {
    RenderThreadImpl::current()->Send(
        new ClipboardHostMsg_WriteObjectsAsync(objects));
  }
}

}  // anonymous namespace

RendererClipboardClient::RendererClipboardClient() {
}

RendererClipboardClient::~RendererClipboardClient() {
}

ui::Clipboard* RendererClipboardClient::GetClipboard() {
  return NULL;
}

uint64 RendererClipboardClient::GetSequenceNumber(
    ui::Clipboard::Buffer buffer) {
  uint64 sequence_number = 0;
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_GetSequenceNumber(buffer,
                                             &sequence_number));
  return sequence_number;
}

bool RendererClipboardClient::IsFormatAvailable(
    const ui::Clipboard::FormatType& format,
    ui::Clipboard::Buffer buffer) {
  bool result;
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_IsFormatAvailable(format, buffer, &result));
  return result;
}

void RendererClipboardClient::ReadAvailableTypes(
    ui::Clipboard::Buffer buffer,
    std::vector<string16>* types,
    bool* contains_filenames) {
  RenderThreadImpl::current()->Send(new ClipboardHostMsg_ReadAvailableTypes(
      buffer, types, contains_filenames));
}

void RendererClipboardClient::ReadText(ui::Clipboard::Buffer buffer,
                                       string16* result) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_ReadText(buffer, result));
}

void RendererClipboardClient::ReadAsciiText(ui::Clipboard::Buffer buffer,
                                            std::string* result) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_ReadAsciiText(buffer, result));
}

void RendererClipboardClient::ReadHTML(ui::Clipboard::Buffer buffer,
                                       string16* markup,
                                       GURL* url, uint32* fragment_start,
                                       uint32* fragment_end) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_ReadHTML(buffer, markup, url, fragment_start,
                                    fragment_end));
}

void RendererClipboardClient::ReadImage(ui::Clipboard::Buffer buffer,
                                        std::string* data) {
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

webkit_glue::ClipboardClient::WriteContext*
RendererClipboardClient::CreateWriteContext() {
  return new RendererClipboardWriteContext;
}
