// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file mocks out just enough of the WebClipboard API for running the
// webkit tests. This is so we can run webkit tests without them sharing a
// clipboard, which allows for running them in parallel and having the tests
// not interact with actual user actions.

#ifndef CONTENT_TEST_MOCK_WEBCLIPBOARD_IMPL_H_
#define CONTENT_TEST_MOCK_WEBCLIPBOARD_IMPL_H_

#include <stdint.h>

#include <map>

#include "base/strings/nullable_string16.h"
#include "base/strings/string16.h"
#include "third_party/WebKit/public/platform/WebDragData.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/WebMockClipboard.h"

namespace content {

class MockWebClipboardImpl : public blink::WebMockClipboard {
 public:
  MockWebClipboardImpl();
  virtual ~MockWebClipboardImpl();

  uint64_t SequenceNumber(blink::mojom::ClipboardBuffer) override;
  bool IsFormatAvailable(blink::mojom::ClipboardFormat format,
                         blink::mojom::ClipboardBuffer buffer) override;
  blink::WebVector<blink::WebString> ReadAvailableTypes(
      blink::mojom::ClipboardBuffer buffer,
      bool* containsFilenames) override;

  blink::WebString ReadPlainText(blink::mojom::ClipboardBuffer buffer) override;
  blink::WebString ReadHTML(blink::mojom::ClipboardBuffer buffer,
                            blink::WebURL* url,
                            unsigned* fragmentStart,
                            unsigned* fragmentEnd) override;
  blink::WebBlobInfo ReadImage(blink::mojom::ClipboardBuffer buffer) override;
  blink::WebImage ReadRawImage(blink::mojom::ClipboardBuffer buffer) override;
  blink::WebString ReadCustomData(blink::mojom::ClipboardBuffer buffer,
                                  const blink::WebString& type) override;

  void WritePlainText(const blink::WebString& plain_text) override;
  void WriteHTML(const blink::WebString& htmlText,
                 const blink::WebURL& url,
                 const blink::WebString& plainText,
                 bool writeSmartPaste) override;
  virtual void writeURL(
      const blink::WebURL& url, const blink::WebString& title);
  void WriteImage(const blink::WebImage& image,
                  const blink::WebURL& url,
                  const blink::WebString& title) override;
  void WriteDataObject(const blink::WebDragData& data) override;

 private:
  void clear();

  uint64_t m_sequenceNumber;
  base::NullableString16 m_plainText;
  base::NullableString16 m_htmlText;
  blink::WebImage m_image;
  std::map<base::string16, base::string16> m_customData;
  bool m_writeSmartPaste;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_WEBCLIPBOARD_IMPL_H_
