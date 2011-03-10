// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "content/browser/browser_message_filter.h"
#include "ui/base/clipboard/clipboard.h"

class GURL;

class ClipboardMessageFilter : public BrowserMessageFilter {
 public:
  ClipboardMessageFilter();

  virtual void OverrideThreadForMessage(const IPC::Message& message,
                                        BrowserThread::ID* thread);
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);
 private:
  ~ClipboardMessageFilter();

  void OnWriteObjectsAsync(const ui::Clipboard::ObjectMap& objects);
  void OnWriteObjectsSync(const ui::Clipboard::ObjectMap& objects,
                          base::SharedMemoryHandle bitmap_handle);

  void OnIsFormatAvailable(ui::Clipboard::FormatType format,
                           ui::Clipboard::Buffer buffer,
                           bool* result);
  void OnReadText(ui::Clipboard::Buffer buffer, string16* result);
  void OnReadAsciiText(ui::Clipboard::Buffer buffer, std::string* result);
  void OnReadHTML(ui::Clipboard::Buffer buffer, string16* markup, GURL* url);
#if defined(OS_MACOSX)
  void OnFindPboardWriteString(const string16& text);
#endif
  void OnReadAvailableTypes(ui::Clipboard::Buffer buffer,
                            bool* succeeded,
                            std::vector<string16>* types,
                            bool* contains_filenames);
  void OnReadData(ui::Clipboard::Buffer buffer, const string16& type,
                  bool* succeeded, string16* data, string16* metadata);
  void OnReadFilenames(ui::Clipboard::Buffer buffer, bool* succeeded,
                       std::vector<string16>* filenames);

  // We have our own clipboard because we want to access the clipboard on the
  // IO thread instead of forwarding (possibly synchronous) messages to the UI
  // thread. This instance of the clipboard should be accessed only on the IO
  // thread.
  static ui::Clipboard* GetClipboard();

  DISALLOW_COPY_AND_ASSIGN(ClipboardMessageFilter);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_MESSAGE_FILTER_H_
