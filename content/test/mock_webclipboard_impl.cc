// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_webclipboard_impl.h"

#include <stddef.h>

#include <algorithm>

#include "base/guid.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/renderer/clipboard_utils.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebBlobRegistry.h"
#include "third_party/WebKit/public/platform/WebCommon.h"
#include "third_party/WebKit/public/platform/WebDragData.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/WebThreadSafeData.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"

using blink::WebDragData;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

MockWebClipboardImpl::MockWebClipboardImpl()
  : m_sequenceNumber(0),
    m_writeSmartPaste(false) {}

MockWebClipboardImpl::~MockWebClipboardImpl() {}

uint64_t MockWebClipboardImpl::sequenceNumber(Buffer) {
  return m_sequenceNumber;
}

bool MockWebClipboardImpl::isFormatAvailable(Format format, Buffer buffer) {
  switch (format) {
    case FormatPlainText:
      return !m_plainText.is_null();

    case FormatHTML:
      return !m_htmlText.is_null();

    case FormatSmartPaste:
      return m_writeSmartPaste;

    default:
      NOTREACHED();
      return false;
  }
}

WebVector<WebString> MockWebClipboardImpl::readAvailableTypes(
    Buffer buffer,
    bool* containsFilenames) {
  *containsFilenames = false;
  std::vector<WebString> results;
  if (!m_plainText.string().empty()) {
    results.push_back(WebString("text/plain"));
  }
  if (!m_htmlText.string().empty()) {
    results.push_back(WebString("text/html"));
  }
  if (!m_image.isNull()) {
    results.push_back(WebString("image/png"));
  }
  for (std::map<base::string16, base::string16>::const_iterator it =
           m_customData.begin();
       it != m_customData.end(); ++it) {
    CHECK(std::find(results.begin(), results.end(), it->first) ==
          results.end());
    results.push_back(it->first);
  }
  return results;
}

blink::WebString MockWebClipboardImpl::readPlainText(
    blink::WebClipboard::Buffer buffer) {
  return m_plainText;
}

// TODO(wtc): set output argument *url.
blink::WebString MockWebClipboardImpl::readHTML(
    blink::WebClipboard::Buffer buffer,
    blink::WebURL* url,
    unsigned* fragmentStart,
    unsigned* fragmentEnd) {
  *fragmentStart = 0;
  *fragmentEnd = static_cast<unsigned>(m_htmlText.string().length());
  return m_htmlText;
}

blink::WebBlobInfo MockWebClipboardImpl::readImage(
    blink::WebClipboard::Buffer buffer) {
  std::vector<unsigned char> output;
  const SkBitmap& bitmap = m_image.getSkBitmap();
  if (!gfx::PNGCodec::FastEncodeBGRASkBitmap(
          bitmap, false /* discard_transparency */, &output)) {
    return blink::WebBlobInfo();
  }
  const WebString& uuid = base::ASCIIToUTF16(base::GenerateGUID());
  std::unique_ptr<blink::WebBlobRegistry::Builder> blob_builder(
      blink::Platform::current()->blobRegistry()->createBuilder(
          uuid, blink::WebString()));
  blob_builder->appendData(blink::WebThreadSafeData(
      reinterpret_cast<char*>(output.data()), output.size()));
  blob_builder->build();
  return blink::WebBlobInfo(
      uuid, base::ASCIIToUTF16(ui::Clipboard::kMimeTypePNG), output.size());
}

blink::WebImage MockWebClipboardImpl::readRawImage(
    blink::WebClipboard::Buffer buffer) {
  return m_image;
}

blink::WebString MockWebClipboardImpl::readCustomData(
    blink::WebClipboard::Buffer buffer,
    const blink::WebString& type) {
  std::map<base::string16, base::string16>::const_iterator it =
      m_customData.find(type);
  if (it != m_customData.end())
    return it->second;
  return blink::WebString();
}

void MockWebClipboardImpl::writeHTML(const blink::WebString& htmlText,
                                     const blink::WebURL& url,
                                     const blink::WebString& plainText,
                                     bool writeSmartPaste) {
  clear();

  m_htmlText = htmlText;
  m_plainText = plainText;
  m_writeSmartPaste = writeSmartPaste;
  ++m_sequenceNumber;
}

void MockWebClipboardImpl::writePlainText(const blink::WebString& plain_text) {
  clear();

  m_plainText = plain_text;
  ++m_sequenceNumber;
}

void MockWebClipboardImpl::writeURL(const blink::WebURL& url,
                                    const blink::WebString& title) {
  clear();

  m_htmlText = WebString::fromUTF8(URLToMarkup(url, title));
  m_plainText = url.string();
  ++m_sequenceNumber;
}

void MockWebClipboardImpl::writeImage(const blink::WebImage& image,
                                      const blink::WebURL& url,
                                      const blink::WebString& title) {
  if (!image.isNull()) {
    clear();

    m_plainText = m_htmlText;
    m_htmlText = WebString::fromUTF8(URLToImageMarkup(url, title));
    m_image = image;
    ++m_sequenceNumber;
  }
}

void MockWebClipboardImpl::writeDataObject(const WebDragData& data) {
  clear();

  const WebVector<WebDragData::Item>& itemList = data.items();
  for (size_t i = 0; i < itemList.size(); ++i) {
    const WebDragData::Item& item = itemList[i];
    switch (item.storageType) {
      case WebDragData::Item::StorageTypeString: {
        ++m_sequenceNumber;
        base::string16 type(item.stringType);
        if (base::EqualsASCII(type, ui::Clipboard::kMimeTypeText)) {
          m_plainText = item.stringData;
          continue;
        }
        if (base::EqualsASCII(type, ui::Clipboard::kMimeTypeHTML)) {
          m_htmlText = item.stringData;
          continue;
        }
        m_customData.insert(std::make_pair(type, item.stringData));
        continue;
      }
      default:
        // Currently other types are unused by the clipboard implementation.
        NOTREACHED();
    }
  }
}

void MockWebClipboardImpl::clear() {
  m_plainText = base::NullableString16();
  m_htmlText = base::NullableString16();
  m_image.reset();
  m_customData.clear();
  m_writeSmartPaste = false;
}

}  // namespace content
