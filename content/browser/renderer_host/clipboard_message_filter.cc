// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/common/clipboard_messages.h"
#include "content/public/browser/browser_context.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace content {

namespace {

void ReleaseSharedMemoryPixels(void* addr, void* context) {
  delete reinterpret_cast<base::SharedMemory*>(context);
}

}  // namespace

ClipboardMessageFilter::ClipboardMessageFilter()
    : BrowserMessageFilter(ClipboardMsgStart),
      clipboard_writer_(
          new ui::ScopedClipboardWriter(ui::CLIPBOARD_TYPE_COPY_PASTE)) {
}

void ClipboardMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  // Clipboard writes should always occur on the UI thread due the restrictions
  // of various platform APIs. In general, the clipboard is not thread-safe, so
  // all clipboard calls should be serviced from the UI thread.
  //
  // Windows needs clipboard reads to be serviced from the IO thread because
  // these are sync IPCs which can result in deadlocks with NPAPI plugins if
  // serviced from the UI thread. Note that Windows clipboard calls ARE
  // thread-safe so it is ok for reads and writes to be serviced from different
  // threads.
#if !defined(OS_WIN)
  if (IPC_MESSAGE_CLASS(message) == ClipboardMsgStart)
    *thread = BrowserThread::UI;
#endif

#if defined(OS_WIN)
  if (message.type() == ClipboardHostMsg_ReadImage::ID)
    *thread = BrowserThread::FILE;
#endif
}

bool ClipboardMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ClipboardMessageFilter, message)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_GetSequenceNumber, OnGetSequenceNumber)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_IsFormatAvailable, OnIsFormatAvailable)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_Clear, OnClear)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadAvailableTypes,
                        OnReadAvailableTypes)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadText, OnReadText)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadHTML, OnReadHTML)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadRTF, OnReadRTF)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ClipboardHostMsg_ReadImage, OnReadImage)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadCustomData, OnReadCustomData)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_WriteText, OnWriteText)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_WriteHTML, OnWriteHTML)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_WriteSmartPasteMarker,
                        OnWriteSmartPasteMarker)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_WriteCustomData, OnWriteCustomData)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_WriteBookmark, OnWriteBookmark)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_WriteImage, OnWriteImage)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_CommitWrite, OnCommitWrite);
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_FindPboardWriteStringAsync,
                        OnFindPboardWriteString)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

ClipboardMessageFilter::~ClipboardMessageFilter() {
  clipboard_writer_->Reset();
}

void ClipboardMessageFilter::OnGetSequenceNumber(ui::ClipboardType type,
                                                 uint64_t* sequence_number) {
  *sequence_number = GetClipboard()->GetSequenceNumber(type);
}

void ClipboardMessageFilter::OnReadAvailableTypes(
    ui::ClipboardType type,
    std::vector<base::string16>* types,
    bool* contains_filenames) {
  GetClipboard()->ReadAvailableTypes(type, types, contains_filenames);
}

void ClipboardMessageFilter::OnIsFormatAvailable(ClipboardFormat format,
                                                 ui::ClipboardType type,
                                                 bool* result) {
  switch (format) {
    case CLIPBOARD_FORMAT_PLAINTEXT:
      *result = GetClipboard()->IsFormatAvailable(
                    ui::Clipboard::GetPlainTextWFormatType(), type) ||
                GetClipboard()->IsFormatAvailable(
                    ui::Clipboard::GetPlainTextFormatType(), type);
      break;
    case CLIPBOARD_FORMAT_HTML:
      *result = GetClipboard()->IsFormatAvailable(
          ui::Clipboard::GetHtmlFormatType(), type);
      break;
    case CLIPBOARD_FORMAT_SMART_PASTE:
      *result = GetClipboard()->IsFormatAvailable(
          ui::Clipboard::GetWebKitSmartPasteFormatType(), type);
      break;
    case CLIPBOARD_FORMAT_BOOKMARK:
#if defined(OS_WIN) || defined(OS_MACOSX)
      *result = GetClipboard()->IsFormatAvailable(
          ui::Clipboard::GetUrlWFormatType(), type);
#else
      *result = false;
#endif
      break;
  }
}

void ClipboardMessageFilter::OnClear(ui::ClipboardType type) {
  GetClipboard()->Clear(type);
}

void ClipboardMessageFilter::OnReadText(ui::ClipboardType type,
                                        base::string16* result) {
  if (GetClipboard()->IsFormatAvailable(
          ui::Clipboard::GetPlainTextWFormatType(), type)) {
    GetClipboard()->ReadText(type, result);
  } else if (GetClipboard()->IsFormatAvailable(
                 ui::Clipboard::GetPlainTextFormatType(), type)) {
    std::string ascii;
    GetClipboard()->ReadAsciiText(type, &ascii);
    *result = base::ASCIIToUTF16(ascii);
  } else {
    result->clear();
  }
}

void ClipboardMessageFilter::OnReadHTML(ui::ClipboardType type,
                                        base::string16* markup,
                                        GURL* url,
                                        uint32_t* fragment_start,
                                        uint32_t* fragment_end) {
  std::string src_url_str;
  GetClipboard()->ReadHTML(type, markup, &src_url_str, fragment_start,
                           fragment_end);
  *url = GURL(src_url_str);
}

void ClipboardMessageFilter::OnReadRTF(ui::ClipboardType type,
                                       std::string* result) {
  GetClipboard()->ReadRTF(type, result);
}

void ClipboardMessageFilter::OnReadImage(ui::ClipboardType type,
                                         IPC::Message* reply_msg) {
  SkBitmap bitmap = GetClipboard()->ReadImage(type);

#if defined(USE_X11)
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &ClipboardMessageFilter::OnReadImageReply, this, bitmap, reply_msg));
#else
  OnReadImageReply(bitmap, reply_msg);
#endif
}

void ClipboardMessageFilter::OnReadImageReply(
    const SkBitmap& bitmap, IPC::Message* reply_msg) {
  base::SharedMemoryHandle image_handle = base::SharedMemory::NULLHandle();
  uint32_t image_size = 0;
  if (!bitmap.isNull()) {
    std::vector<unsigned char> png_data;
    if (gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, false, &png_data)) {
      base::SharedMemory buffer;
      if (buffer.CreateAndMapAnonymous(png_data.size())) {
        memcpy(buffer.memory(), png_data.data(), png_data.size());
        if (buffer.GiveToProcess(PeerHandle(), &image_handle)) {
          image_size = png_data.size();
        }
      }
    }
  }
  ClipboardHostMsg_ReadImage::WriteReplyParams(reply_msg, image_handle,
                                               image_size);
  Send(reply_msg);
}

void ClipboardMessageFilter::OnReadCustomData(ui::ClipboardType clipboard_type,
                                              const base::string16& type,
                                              base::string16* result) {
  GetClipboard()->ReadCustomData(clipboard_type, type, result);
}

void ClipboardMessageFilter::OnWriteText(ui::ClipboardType clipboard_type,
                                         const base::string16& text) {
  clipboard_writer_->WriteText(text);
}

void ClipboardMessageFilter::OnWriteHTML(ui::ClipboardType clipboard_type,
                                         const base::string16& markup,
                                         const GURL& url) {
  clipboard_writer_->WriteHTML(markup, url.spec());
}

void ClipboardMessageFilter::OnWriteSmartPasteMarker(
    ui::ClipboardType clipboard_type) {
  clipboard_writer_->WriteWebSmartPaste();
}

void ClipboardMessageFilter::OnWriteCustomData(
    ui::ClipboardType clipboard_type,
    const std::map<base::string16, base::string16>& data) {
  base::Pickle pickle;
  ui::WriteCustomDataToPickle(data, &pickle);
  clipboard_writer_->WritePickledData(
      pickle, ui::Clipboard::GetWebCustomDataFormatType());
}

void ClipboardMessageFilter::OnWriteBookmark(ui::ClipboardType clipboard_type,
                                             const std::string& url,
                                             const base::string16& title) {
  clipboard_writer_->WriteBookmark(title, url);
}

void ClipboardMessageFilter::OnWriteImage(ui::ClipboardType clipboard_type,
                                          const gfx::Size& size,
                                          base::SharedMemoryHandle handle) {
  if (!base::SharedMemory::IsHandleValid(handle)) {
    return;
  }

  scoped_ptr<base::SharedMemory> bitmap_buffer(
#if defined(OS_WIN)
      new base::SharedMemory(handle, true, PeerHandle()));
#else
      new base::SharedMemory(handle, true));
#endif

  SkBitmap bitmap;
  // Let Skia do some sanity checking for (no negative widths/heights, no
  // overflows while calculating bytes per row, etc).
  if (!bitmap.setInfo(
          SkImageInfo::MakeN32Premul(size.width(), size.height()))) {
    return;
  }

  // Make sure the size is representable as a signed 32-bit int, so
  // SkBitmap::getSize() won't be truncated.
  if (!sk_64_isS32(bitmap.computeSize64()))
    return;

  if (!bitmap_buffer->Map(bitmap.getSize()))
    return;

  if (!bitmap.installPixels(bitmap.info(), bitmap_buffer->memory(),
                            bitmap.rowBytes(), NULL, &ReleaseSharedMemoryPixels,
                            bitmap_buffer.get()))
    return;

  // On success, SkBitmap now owns the SharedMemory.
  ignore_result(bitmap_buffer.release());
  clipboard_writer_->WriteImage(bitmap);
}

void ClipboardMessageFilter::OnCommitWrite(ui::ClipboardType clipboard_type) {
#if defined(OS_WIN)
  // On non-Windows platforms, all clipboard IPCs are handled on the UI thread.
  // However, Windows handles the clipboard IPCs on the IO thread to prevent
  // deadlocks. Clipboard writes must still occur on the UI thread because the
  // clipboard object from the IO thread cannot create windows so it cannot be
  // the "owner" of the clipboard's contents. See http://crbug.com/5823.
  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE,
                            clipboard_writer_.release());
#endif
  clipboard_writer_.reset(
      new ui::ScopedClipboardWriter(ui::CLIPBOARD_TYPE_COPY_PASTE));
}

// static
ui::Clipboard* ClipboardMessageFilter::GetClipboard() {
  // We have a static instance of the clipboard service for use by all message
  // filters.  This instance lives for the life of the browser processes.
  static ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  return clipboard;
}

}  // namespace content
