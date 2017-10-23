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

uint64_t MockWebClipboardImpl::SequenceNumber(Buffer) {
  return m_sequenceNumber;
}

bool MockWebClipboardImpl::IsFormatAvailable(Format format, Buffer buffer) {
  switch (format) {
    case kFormatPlainText:
      return !m_plainText.is_null();

    case kFormatHTML:
      return !m_htmlText.is_null();

    case kFormatSmartPaste:
      return m_writeSmartPaste;

    default:
      NOTREACHED();
      return false;
  }
}

WebVector<WebString> MockWebClipboardImpl::ReadAvailableTypes(
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
  if (!m_image.IsNull()) {
    results.push_back(WebString("image/png"));
  }
  for (std::map<base::string16, base::string16>::const_iterator it =
           m_customData.begin();
       it != m_customData.end(); ++it) {
    CHECK(std::find(results.begin(), results.end(),
                    WebString::FromUTF16(it->first)) == results.end());
    results.push_back(WebString::FromUTF16(it->first));
  }
  return results;
}

blink::WebString MockWebClipboardImpl::ReadPlainText(
    blink::WebClipboard::Buffer buffer) {
  return WebString::FromUTF16(m_plainText);
}

// TODO(wtc): set output argument *url.
blink::WebString MockWebClipboardImpl::ReadHTML(
    blink::WebClipboard::Buffer buffer,
    blink::WebURL* url,
    unsigned* fragmentStart,
    unsigned* fragmentEnd) {
  *fragmentStart = 0;
  *fragmentEnd = static_cast<unsigned>(m_htmlText.string().length());
  return WebString::FromUTF16(m_htmlText);
}

blink::WebBlobInfo MockWebClipboardImpl::ReadImage(
    blink::WebClipboard::Buffer buffer) {
  std::vector<unsigned char> output;
  const SkBitmap& bitmap = m_image.GetSkBitmap();
  if (!gfx::PNGCodec::FastEncodeBGRASkBitmap(
          bitmap, false /* discard_transparency */, &output)) {
    return blink::WebBlobInfo();
  }
  return CreateBlobFromData(output,
                            WebString::FromASCII(ui::Clipboard::kMimeTypePNG));
}

blink::WebImage MockWebClipboardImpl::ReadRawImage(
    blink::WebClipboard::Buffer buffer) {
  return m_image;
}

blink::WebString MockWebClipboardImpl::ReadCustomData(
    blink::WebClipboard::Buffer buffer,
    const blink::WebString& type) {
  std::map<base::string16, base::string16>::const_iterator it =
      m_customData.find(type.Utf16());
  if (it != m_customData.end())
    return WebString::FromUTF16(it->second);
  return blink::WebString();
}

void MockWebClipboardImpl::WriteHTML(const blink::WebString& htmlText,
                                     const blink::WebURL& url,
                                     const blink::WebString& plainText,
                                     bool writeSmartPaste) {
  clear();

  m_htmlText = WebString::ToNullableString16(htmlText);
  m_plainText = WebString::ToNullableString16(plainText);
  m_writeSmartPaste = writeSmartPaste;
  ++m_sequenceNumber;
}

void MockWebClipboardImpl::WritePlainText(const blink::WebString& plain_text) {
  clear();

  m_plainText = WebString::ToNullableString16(plain_text);
  ++m_sequenceNumber;
}

void MockWebClipboardImpl::writeURL(const blink::WebURL& url,
                                    const blink::WebString& title) {
  clear();

  m_htmlText = base::NullableString16(
      base::UTF8ToUTF16(URLToMarkup(url, title)), false /* is_null */);
  m_plainText = WebString::ToNullableString16(url.GetString());
  ++m_sequenceNumber;
}

void MockWebClipboardImpl::WriteImage(const blink::WebImage& image,
                                      const blink::WebURL& url,
                                      const blink::WebString& title) {
  if (!image.IsNull()) {
    clear();

    m_plainText = m_htmlText;
    m_htmlText = base::NullableString16(
        base::UTF8ToUTF16(URLToImageMarkup(url, title)), false /* is_null */);
    m_image = image;
    ++m_sequenceNumber;
  }
}

void MockWebClipboardImpl::WriteDataObject(const WebDragData& data) {
  clear();

  const WebVector<WebDragData::Item>& itemList = data.Items();
  for (size_t i = 0; i < itemList.size(); ++i) {
    const WebDragData::Item& item = itemList[i];
    switch (item.storage_type) {
      case WebDragData::Item::kStorageTypeString: {
        ++m_sequenceNumber;
        base::string16 type(item.string_type.Utf16());
        if (base::EqualsASCII(type, ui::Clipboard::kMimeTypeText)) {
          m_plainText = WebString::ToNullableString16(item.string_data);
          continue;
        }
        if (base::EqualsASCII(type, ui::Clipboard::kMimeTypeHTML)) {
          m_htmlText = WebString::ToNullableString16(item.string_data);
          continue;
        }
        m_customData.insert(std::make_pair(type, item.string_data.Utf16()));
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
  m_image.Reset();
  m_customData.clear();
  m_writeSmartPaste = false;
}

}  // namespace content
