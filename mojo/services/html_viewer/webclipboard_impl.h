// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_WEBCLIPBOARD_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_WEBCLIPBOARD_IMPL_H_

#include "third_party/WebKit/public/platform/WebClipboard.h"
#include "third_party/mojo_services/src/clipboard/public/interfaces/clipboard.mojom.h"

namespace html_viewer {

class WebClipboardImpl : public blink::WebClipboard {
 public:
  WebClipboardImpl(mojo::ClipboardPtr clipboard);
  virtual ~WebClipboardImpl();

  // blink::WebClipboard methods:
  uint64_t sequenceNumber(Buffer) override;
  bool isFormatAvailable(Format, Buffer) override;
  blink::WebVector<blink::WebString> readAvailableTypes(
      Buffer,
      bool* containsFilenames) override;
  blink::WebString readPlainText(Buffer) override;
  blink::WebString readHTML(Buffer buffer,
                            blink::WebURL* pageURL,
                            unsigned* fragmentStart,
                            unsigned* fragmentEnd) override;
  // TODO(erg): readImage()
  blink::WebString readCustomData(Buffer,
                                  const blink::WebString& type) override;
  void writePlainText(const blink::WebString&) override;
  void writeHTML(const blink::WebString& htmlText,
                 const blink::WebURL&,
                 const blink::WebString& plainText,
                 bool writeSmartPaste) override;

 private:
  // Changes webkit buffers to mojo Clipboard::Types.
  mojo::Clipboard::Type ConvertBufferType(Buffer buffer);

  mojo::ClipboardPtr clipboard_;

  DISALLOW_COPY_AND_ASSIGN(WebClipboardImpl);
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_WEBCLIPBOARD_IMPL_H_
