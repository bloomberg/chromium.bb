// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "content/public/browser/browser_message_filter.h"
#include "ui/base/clipboard/clipboard.h"

class GURL;

namespace content {

class BrowserContext;

class ClipboardMessageFilter : public BrowserMessageFilter {
 public:
  ClipboardMessageFilter(BrowserContext* browser_context);

  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;
 private:
  virtual ~ClipboardMessageFilter();

  void OnWriteObjectsAsync(const ui::Clipboard::ObjectMap& objects);
  void OnWriteObjectsSync(ui::Clipboard::ObjectMap objects,
                          base::SharedMemoryHandle bitmap_handle);

  void OnGetSequenceNumber(const ui::Clipboard::Buffer buffer,
                           uint64* sequence_number);
  void OnIsFormatAvailable(const ui::Clipboard::FormatType& format,
                           ui::Clipboard::Buffer buffer,
                           bool* result);
  void OnClear(ui::Clipboard::Buffer buffer);
  void OnReadAvailableTypes(ui::Clipboard::Buffer buffer,
                            std::vector<string16>* types,
                            bool* contains_filenames);
  void OnReadText(ui::Clipboard::Buffer buffer, string16* result);
  void OnReadAsciiText(ui::Clipboard::Buffer buffer, std::string* result);
  void OnReadHTML(ui::Clipboard::Buffer buffer, string16* markup, GURL* url,
                  uint32* fragment_start, uint32* fragment_end);
  void OnReadRTF(ui::Clipboard::Buffer buffer, std::string* result);
  void OnReadImage(ui::Clipboard::Buffer buffer, IPC::Message* reply_msg);
  void OnReadImageReply(const SkBitmap& bitmap, IPC::Message* reply_msg);
  void OnReadCustomData(ui::Clipboard::Buffer buffer,
                        const string16& type,
                        string16* result);
  void OnReadData(const ui::Clipboard::FormatType& format,
                  std::string* data);

#if defined(OS_MACOSX)
  void OnFindPboardWriteString(const string16& text);
#endif

  // We have our own clipboard because we want to access the clipboard on the
  // IO thread instead of forwarding (possibly synchronous) messages to the UI
  // thread. This instance of the clipboard should be accessed only on the IO
  // thread.
  static ui::Clipboard* GetClipboard();

  BrowserContext* const browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_MESSAGE_FILTER_H_
