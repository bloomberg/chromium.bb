// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_WEBCLIPBOARD_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_WEBCLIPBOARD_IMPL_H_

#include "mojo/services/public/interfaces/clipboard/clipboard.mojom.h"
#include "third_party/WebKit/public/platform/WebClipboard.h"

namespace mojo {

class WebClipboardImpl : public blink::WebClipboard {
 public:
  WebClipboardImpl(ClipboardPtr clipboard);
  virtual ~WebClipboardImpl();

  // blink::WebClipboard methods:
  virtual uint64_t sequenceNumber(Buffer);
  virtual bool isFormatAvailable(Format, Buffer);
  virtual blink::WebVector<blink::WebString> readAvailableTypes(
      Buffer,
      bool* containsFilenames);
  virtual blink::WebString readPlainText(Buffer);
  virtual blink::WebString readHTML(Buffer buffer,
                                    blink::WebURL* pageURL,
                                    unsigned* fragmentStart,
                                    unsigned* fragmentEnd);
  // TODO(erg): readImage()
  virtual blink::WebString readCustomData(Buffer, const blink::WebString& type);
  virtual void writePlainText(const blink::WebString&);
  virtual void writeHTML(const blink::WebString& htmlText,
                         const blink::WebURL&,
                         const blink::WebString& plainText,
                         bool writeSmartPaste);

 private:
  // Changes webkit buffers to mojo Clipboard::Types.
  mojo::Clipboard::Type ConvertBufferType(Buffer buffer);

  ClipboardPtr clipboard_;

  DISALLOW_COPY_AND_ASSIGN(WebClipboardImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_HTML_VIEWER_WEBCLIPBOARD_IMPL_H_
