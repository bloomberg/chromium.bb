// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBCLIPBOARD_IMPL_H_
#define CONTENT_RENDERER_WEBCLIPBOARD_IMPL_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "content/common/clipboard.mojom.h"
#include "third_party/WebKit/public/platform/WebClipboard.h"
#include "ui/base/clipboard/clipboard.h"

namespace content {

class WebClipboardImpl : public blink::WebClipboard {
 public:
  explicit WebClipboardImpl(mojom::ClipboardHost& clipboard);

  virtual ~WebClipboardImpl();

  // WebClipboard methods:
  uint64_t SequenceNumber(Buffer buffer) override;
  bool IsFormatAvailable(Format format, Buffer buffer) override;
  blink::WebVector<blink::WebString> ReadAvailableTypes(
      Buffer buffer,
      bool* contains_filenames) override;
  blink::WebString ReadPlainText(Buffer buffer) override;
  blink::WebString ReadHTML(Buffer buffer,
                            blink::WebURL* source_url,
                            unsigned* fragment_start,
                            unsigned* fragment_end) override;
  blink::WebString ReadRTF(Buffer buffer) override;
  blink::WebBlobInfo ReadImage(Buffer buffer) override;
  blink::WebString ReadCustomData(Buffer buffer,
                                  const blink::WebString& type) override;
  void WritePlainText(const blink::WebString& plain_text) override;
  void WriteHTML(const blink::WebString& html_text,
                 const blink::WebURL& source_url,
                 const blink::WebString& plain_text,
                 bool write_smart_paste) override;
  void WriteImage(const blink::WebImage& image,
                  const blink::WebURL& source_url,
                  const blink::WebString& title) override;
  void WriteDataObject(const blink::WebDragData& data) override;

 private:
  bool ConvertBufferType(Buffer, ui::ClipboardType*);
  bool WriteImageToClipboard(ui::ClipboardType clipboard_type,
                             const SkBitmap& bitmap);
  mojom::ClipboardHost& clipboard_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBCLIPBOARD_IMPL_H_
