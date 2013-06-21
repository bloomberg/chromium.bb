// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBCLIPBOARD_IMPL_H_
#define CONTENT_RENDERER_WEBCLIPBOARD_IMPL_H_

#include "base/compiler_specific.h"

#include "third_party/WebKit/public/platform/WebClipboard.h"
#include "ui/base/clipboard/clipboard.h"

#include <string>

namespace content {
class ClipboardClient;

class WebClipboardImpl : public WebKit::WebClipboard {
 public:
  explicit WebClipboardImpl(ClipboardClient* client);

  virtual ~WebClipboardImpl();

  // WebClipboard methods:
  virtual uint64 getSequenceNumber();
  virtual uint64 sequenceNumber(Buffer buffer);
  virtual bool isFormatAvailable(Format format, Buffer buffer);
  virtual WebKit::WebVector<WebKit::WebString> readAvailableTypes(
      Buffer buffer, bool* contains_filenames);
  virtual WebKit::WebString readPlainText(Buffer buffer);
  virtual WebKit::WebString readHTML(
      Buffer buffer,
      WebKit::WebURL* source_url,
      unsigned* fragment_start,
      unsigned* fragment_end);
  virtual WebKit::WebData readImage(Buffer buffer);
  virtual WebKit::WebString readCustomData(
      Buffer buffer, const WebKit::WebString& type);
  virtual void writeHTML(
      const WebKit::WebString& html_text,
      const WebKit::WebURL& source_url,
      const WebKit::WebString& plain_text,
      bool write_smart_paste);
  virtual void writePlainText(const WebKit::WebString& plain_text);
  virtual void writeURL(
      const WebKit::WebURL& url,
      const WebKit::WebString& title);
  virtual void writeImage(
      const WebKit::WebImage& image,
      const WebKit::WebURL& source_url,
      const WebKit::WebString& title);
  virtual void writeDataObject(const WebKit::WebDragData& data);

 private:
  bool ConvertBufferType(Buffer, ui::Clipboard::Buffer*);
  ClipboardClient* client_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBCLIPBOARD_IMPL_H_

