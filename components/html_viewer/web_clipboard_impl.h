// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_WEB_CLIPBOARD_IMPL_H_
#define COMPONENTS_HTML_VIEWER_WEB_CLIPBOARD_IMPL_H_

#include "components/clipboard/public/interfaces/clipboard.mojom.h"
#include "third_party/WebKit/public/platform/WebClipboard.h"

namespace html_viewer {

class WebClipboardImpl : public blink::WebClipboard {
 public:
  WebClipboardImpl(mojo::ClipboardPtr clipboard);
  virtual ~WebClipboardImpl();

  // blink::WebClipboard methods:
  virtual uint64_t sequenceNumber(Buffer);
  virtual bool isFormatAvailable(Format, Buffer);
  virtual blink::WebVector<blink::WebString> readAvailableTypes(
      Buffer buffer,
      bool* contains_filenames);
  virtual blink::WebString readPlainText(Buffer buffer);
  virtual blink::WebString readHTML(Buffer buffer,
                                    blink::WebURL* page_url,
                                    unsigned* fragment_start,
                                    unsigned* fragment_end);
  // TODO(erg): readImage()
  virtual blink::WebString readCustomData(Buffer buffer,
                                          const blink::WebString& type);
  virtual void writePlainText(const blink::WebString& plain_text);
  virtual void writeHTML(const blink::WebString& html_text,
                         const blink::WebURL& source_url,
                         const blink::WebString& plain_text,
                         bool write_smart_paste);

 private:
  // Changes webkit buffers to mojo Clipboard::Types.
  mojo::Clipboard::Type ConvertBufferType(Buffer buffer);

  mojo::ClipboardPtr clipboard_;

  DISALLOW_COPY_AND_ASSIGN(WebClipboardImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_CLIPBOARD_IMPL_H_
