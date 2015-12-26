// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_WEB_CLIPBOARD_IMPL_H_
#define COMPONENTS_HTML_VIEWER_WEB_CLIPBOARD_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/clipboard/public/interfaces/clipboard.mojom.h"
#include "third_party/WebKit/public/platform/WebClipboard.h"

namespace html_viewer {

class WebClipboardImpl : public blink::WebClipboard {
 public:
  WebClipboardImpl(mojo::ClipboardPtr clipboard);
  virtual ~WebClipboardImpl();

  // blink::WebClipboard methods:
  uint64_t sequenceNumber(Buffer) override;
  bool isFormatAvailable(Format, Buffer) override;
  blink::WebVector<blink::WebString> readAvailableTypes(
      Buffer buffer,
      bool* contains_filenames) override;
  blink::WebString readPlainText(Buffer buffer) override;
  blink::WebString readHTML(Buffer buffer,
                            blink::WebURL* page_url,
                            unsigned* fragment_start,
                            unsigned* fragment_end) override;
  // TODO(erg): readImage()
  blink::WebString readCustomData(Buffer buffer,
                                  const blink::WebString& type) override;
  void writePlainText(const blink::WebString& plain_text) override;
  void writeHTML(const blink::WebString& html_text,
                 const blink::WebURL& source_url,
                 const blink::WebString& plain_text,
                 bool write_smart_paste) override;

 private:
  // Changes webkit buffers to mojo Clipboard::Types.
  mojo::Clipboard::Type ConvertBufferType(Buffer buffer);

  mojo::ClipboardPtr clipboard_;

  DISALLOW_COPY_AND_ASSIGN(WebClipboardImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_CLIPBOARD_IMPL_H_
