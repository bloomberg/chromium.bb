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

  uint64_t sequenceNumber(Buffer) override;
  bool isFormatAvailable(blink::WebClipboard::Format format,
                         blink::WebClipboard::Buffer buffer) override;
  blink::WebVector<blink::WebString> readAvailableTypes(
      blink::WebClipboard::Buffer buffer,
      bool* containsFilenames) override;

  blink::WebString readPlainText(blink::WebClipboard::Buffer buffer) override;
  blink::WebString readHTML(blink::WebClipboard::Buffer buffer,
                            blink::WebURL* url,
                            unsigned* fragmentStart,
                            unsigned* fragmentEnd) override;
  blink::WebBlobInfo readImage(blink::WebClipboard::Buffer buffer) override;
  blink::WebImage readRawImage(blink::WebClipboard::Buffer buffer) override;
  blink::WebString readCustomData(blink::WebClipboard::Buffer buffer,
                                  const blink::WebString& type) override;

  void writePlainText(const blink::WebString& plain_text) override;
  void writeHTML(const blink::WebString& htmlText,
                 const blink::WebURL& url,
                 const blink::WebString& plainText,
                 bool writeSmartPaste) override;
  virtual void writeURL(
      const blink::WebURL& url, const blink::WebString& title);
  void writeImage(const blink::WebImage& image,
                  const blink::WebURL& url,
                  const blink::WebString& title) override;
  void writeDataObject(const blink::WebDragData& data) override;

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
