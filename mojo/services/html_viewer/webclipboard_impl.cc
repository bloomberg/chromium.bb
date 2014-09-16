// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/webclipboard_impl.h"

#include "base/bind.h"
#include "mojo/services/html_viewer/blink_basic_type_converters.h"

namespace mojo {
namespace {

void CopyUint64(uint64_t* output, uint64_t input) {
  *output = input;
}

void CopyWebString(blink::WebString* output,
                   const mojo::Array<uint8_t>& input) {
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

void CopyURL(blink::WebURL* pageURL,
             const mojo::Array<uint8_t>& input) {
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

WebClipboardImpl::WebClipboardImpl(ClipboardPtr clipboard)
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
  clipboard_.WaitForIncomingMethodCall();
  return number;
}

bool WebClipboardImpl::isFormatAvailable(Format format, Buffer buffer) {
  mojo::Clipboard::Type clipboard_type = ConvertBufferType(buffer);

  std::vector<std::string> types;
  clipboard_->GetAvailableMimeTypes(
      clipboard_type, base::Bind(&CopyVectorString, &types));

  // Force this to be synchronous.
  clipboard_.WaitForIncomingMethodCall();

  switch (format) {
    case FormatPlainText:
      return Contains(types, mojo::Clipboard::MIME_TYPE_TEXT);
    case FormatHTML:
      return Contains(types, mojo::Clipboard::MIME_TYPE_HTML);
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
    bool* containsFilenames) {
  mojo::Clipboard::Type clipboard_type = ConvertBufferType(buffer);

  std::vector<std::string> types;
  clipboard_->GetAvailableMimeTypes(
      clipboard_type, base::Bind(&CopyVectorString, &types));

  // Force this to be synchronous.
  clipboard_.WaitForIncomingMethodCall();

  // AFAICT, every instance of setting containsFilenames is false.
  *containsFilenames = false;

  blink::WebVector<blink::WebString> output(types.size());
  for (size_t i = 0; i < types.size(); ++i) {
    output[i] = blink::WebString::fromUTF8(types[i]);
  }

  return output;
}

blink::WebString WebClipboardImpl::readPlainText(Buffer buffer) {
  mojo::Clipboard::Type type = ConvertBufferType(buffer);

  blink::WebString text;
  clipboard_->ReadMimeType(
      type, mojo::Clipboard::MIME_TYPE_TEXT, base::Bind(&CopyWebString, &text));

  // Force this to be synchronous.
  clipboard_.WaitForIncomingMethodCall();

  return text;
}

blink::WebString WebClipboardImpl::readHTML(Buffer buffer,
                                            blink::WebURL* pageURL,
                                            unsigned* fragmentStart,
                                            unsigned* fragmentEnd) {
  mojo::Clipboard::Type type = ConvertBufferType(buffer);

  blink::WebString html;
  clipboard_->ReadMimeType(
      type, mojo::Clipboard::MIME_TYPE_HTML, base::Bind(&CopyWebString, &html));
  clipboard_.WaitForIncomingMethodCall();

  *fragmentStart = 0;
  *fragmentEnd = static_cast<unsigned>(html.length());

  clipboard_->ReadMimeType(
      type, mojo::Clipboard::MIME_TYPE_URL, base::Bind(&CopyURL, pageURL));
  clipboard_.WaitForIncomingMethodCall();

  return html;
}

blink::WebString WebClipboardImpl::readCustomData(
    Buffer buffer,
    const blink::WebString& mime_type) {
  mojo::Clipboard::Type clipboard_type = ConvertBufferType(buffer);

  blink::WebString data;
  clipboard_->ReadMimeType(
      clipboard_type, mime_type.utf8(), base::Bind(&CopyWebString, &data));

  // Force this to be synchronous.
  clipboard_.WaitForIncomingMethodCall();

  return data;
}

void WebClipboardImpl::writePlainText(const blink::WebString& text) {
  Array<MimeTypePairPtr> data;
  MimeTypePairPtr text_data(MimeTypePair::New());
  text_data->mime_type = mojo::Clipboard::MIME_TYPE_TEXT;
  text_data->data = Array<uint8_t>::From(text).Pass();
  data.push_back(text_data.Pass());

  clipboard_->WriteClipboardData(mojo::Clipboard::TYPE_COPY_PASTE, data.Pass());
}

void WebClipboardImpl::writeHTML(const blink::WebString& htmlText,
                                 const blink::WebURL& url,
                                 const blink::WebString& plainText,
                                 bool writeSmartPaste) {
  Array<MimeTypePairPtr> data;
  MimeTypePairPtr text_data(MimeTypePair::New());
  text_data->mime_type = mojo::Clipboard::MIME_TYPE_TEXT;
  text_data->data = Array<uint8_t>::From(plainText).Pass();
  data.push_back(text_data.Pass());

  MimeTypePairPtr html_data(MimeTypePair::New());
  text_data->mime_type = mojo::Clipboard::MIME_TYPE_HTML;
  text_data->data = Array<uint8_t>::From(htmlText).Pass();
  data.push_back(html_data.Pass());

  MimeTypePairPtr url_data(MimeTypePair::New());
  url_data->mime_type = mojo::Clipboard::MIME_TYPE_URL;
  url_data->data = Array<uint8_t>::From(url.string()).Pass();
  data.push_back(url_data.Pass());

  if (writeSmartPaste) {
    MimeTypePairPtr smart_paste(MimeTypePair::New());
    url_data->mime_type = kMimeTypeWebkitSmartPaste;
    url_data->data = Array<uint8_t>::From(blink::WebString()).Pass();
    data.push_back(smart_paste.Pass());
  }

  clipboard_->WriteClipboardData(mojo::Clipboard::TYPE_COPY_PASTE, data.Pass());
}

mojo::Clipboard::Type WebClipboardImpl::ConvertBufferType(Buffer buffer) {
  switch (buffer) {
    case BufferStandard:
      return mojo::Clipboard::TYPE_COPY_PASTE;
    case BufferSelection:
      return mojo::Clipboard::TYPE_SELECTION;
  }

  NOTREACHED();
  return mojo::Clipboard::TYPE_COPY_PASTE;
}

}  // namespace mojo
