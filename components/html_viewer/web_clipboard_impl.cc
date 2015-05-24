// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/web_clipboard_impl.h"

#include "base/bind.h"
#include "components/html_viewer/blink_basic_type_converters.h"

using mojo::Array;
using mojo::Clipboard;
using mojo::Map;
using mojo::String;

namespace html_viewer {
namespace {

void CopyUint64(uint64_t* output, uint64_t input) {
  *output = input;
}

void CopyWebString(blink::WebString* output, const Array<uint8_t>& input) {
  // blink does not differentiate between the requested data type not existing
  // and the empty string.
  if (input.is_null()) {
    output->reset();
  } else {
    *output = blink::WebString::fromUTF8(
        reinterpret_cast<const char*>(&input.front()),
        input.size());
  }
}

void CopyURL(blink::WebURL* pageURL, const Array<uint8_t>& input) {
  if (input.is_null()) {
    *pageURL = blink::WebURL();
  } else {
    *pageURL = GURL(std::string(reinterpret_cast<const char*>(&input.front()),
                                input.size()));
  }
}

void CopyVectorString(std::vector<std::string>* output,
                      const Array<String>& input) {
  *output = input.To<std::vector<std::string> >();
}

template <typename T, typename U>
bool Contains(const std::vector<T>& v, const U& item) {
  return std::find(v.begin(), v.end(), item) != v.end();
}

const char kMimeTypeWebkitSmartPaste[] = "chromium/x-webkit-paste";

}  // namespace

WebClipboardImpl::WebClipboardImpl(mojo::ClipboardPtr clipboard)
    : clipboard_(clipboard.Pass()) {
}

WebClipboardImpl::~WebClipboardImpl() {
}

uint64_t WebClipboardImpl::sequenceNumber(Buffer buffer) {
  mojo::Clipboard::Type clipboard_type = ConvertBufferType(buffer);

  uint64_t number = 0;
  clipboard_->GetSequenceNumber(clipboard_type,
                                base::Bind(&CopyUint64, &number));

  // Force this to be synchronous.
  clipboard_.WaitForIncomingResponse();
  return number;
}

bool WebClipboardImpl::isFormatAvailable(Format format, Buffer buffer) {
  Clipboard::Type clipboard_type = ConvertBufferType(buffer);

  std::vector<std::string> types;
  clipboard_->GetAvailableMimeTypes(
      clipboard_type, base::Bind(&CopyVectorString, &types));

  // Force this to be synchronous.
  clipboard_.WaitForIncomingResponse();

  switch (format) {
    case FormatPlainText:
      return Contains(types, Clipboard::MIME_TYPE_TEXT);
    case FormatHTML:
      return Contains(types, Clipboard::MIME_TYPE_HTML);
    case FormatSmartPaste:
      return Contains(types, kMimeTypeWebkitSmartPaste);
    case FormatBookmark:
      // This might be difficult.
      return false;
  }

  return false;
}

blink::WebVector<blink::WebString> WebClipboardImpl::readAvailableTypes(
    Buffer buffer,
    bool* contains_filenames) {
  Clipboard::Type clipboard_type = ConvertBufferType(buffer);

  std::vector<std::string> types;
  clipboard_->GetAvailableMimeTypes(
      clipboard_type, base::Bind(&CopyVectorString, &types));

  // Force this to be synchronous.
  clipboard_.WaitForIncomingResponse();

  // AFAICT, every instance of setting contains_filenames is false.
  *contains_filenames = false;

  blink::WebVector<blink::WebString> output(types.size());
  for (size_t i = 0; i < types.size(); ++i) {
    output[i] = blink::WebString::fromUTF8(types[i]);
  }

  return output;
}

blink::WebString WebClipboardImpl::readPlainText(Buffer buffer) {
  Clipboard::Type type = ConvertBufferType(buffer);

  blink::WebString text;
  clipboard_->ReadMimeType(type, Clipboard::MIME_TYPE_TEXT,
                           base::Bind(&CopyWebString, &text));

  // Force this to be synchronous.
  clipboard_.WaitForIncomingResponse();

  return text;
}

blink::WebString WebClipboardImpl::readHTML(Buffer buffer,
                                            blink::WebURL* page_url,
                                            unsigned* fragment_start,
                                            unsigned* fragment_end) {
  Clipboard::Type type = ConvertBufferType(buffer);

  blink::WebString html;
  clipboard_->ReadMimeType(type, Clipboard::MIME_TYPE_HTML,
                           base::Bind(&CopyWebString, &html));
  clipboard_.WaitForIncomingResponse();

  *fragment_start = 0;
  *fragment_end = static_cast<unsigned>(html.length());

  clipboard_->ReadMimeType(type, Clipboard::MIME_TYPE_URL,
                           base::Bind(&CopyURL, page_url));
  clipboard_.WaitForIncomingResponse();

  return html;
}

blink::WebString WebClipboardImpl::readCustomData(
    Buffer buffer,
    const blink::WebString& mime_type) {
  Clipboard::Type clipboard_type = ConvertBufferType(buffer);

  blink::WebString data;
  clipboard_->ReadMimeType(
      clipboard_type, mime_type.utf8(), base::Bind(&CopyWebString, &data));

  // Force this to be synchronous.
  clipboard_.WaitForIncomingResponse();

  return data;
}

void WebClipboardImpl::writePlainText(const blink::WebString& plain_text) {
  Map<String, Array<uint8_t>> data;
  data[Clipboard::MIME_TYPE_TEXT] = Array<uint8_t>::From(plain_text);

  clipboard_->WriteClipboardData(Clipboard::TYPE_COPY_PASTE, data.Pass());
}

void WebClipboardImpl::writeHTML(const blink::WebString& html_text,
                                 const blink::WebURL& source_url,
                                 const blink::WebString& plain_text,
                                 bool writeSmartPaste) {
  Map<String, Array<uint8_t>> data;
  data[Clipboard::MIME_TYPE_TEXT] = Array<uint8_t>::From(plain_text);
  data[Clipboard::MIME_TYPE_HTML] = Array<uint8_t>::From(html_text);
  data[Clipboard::MIME_TYPE_URL] = Array<uint8_t>::From(source_url.string());

  if (writeSmartPaste)
    data[kMimeTypeWebkitSmartPaste] = Array<uint8_t>::From(blink::WebString());

  clipboard_->WriteClipboardData(Clipboard::TYPE_COPY_PASTE, data.Pass());
}

Clipboard::Type WebClipboardImpl::ConvertBufferType(Buffer buffer) {
  switch (buffer) {
    case BufferStandard:
      return Clipboard::TYPE_COPY_PASTE;
    case BufferSelection:
      return Clipboard::TYPE_SELECTION;
  }

  NOTREACHED();
  return Clipboard::TYPE_COPY_PASTE;
}

}  // namespace html_viewer
