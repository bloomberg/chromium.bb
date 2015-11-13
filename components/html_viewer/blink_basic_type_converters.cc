// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/blink_basic_type_converters.h"

#include "mojo/public/cpp/bindings/string.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebString.h"

using blink::WebRect;
using blink::WebString;

namespace mojo {

// static
String TypeConverter<String, WebString>::Convert(const WebString& str) {
  return String(str.utf8());
}

// static
WebString TypeConverter<WebString, String>::Convert(const String& str) {
  return WebString::fromUTF8(str.get());
}

// static
WebString TypeConverter<WebString, Array<uint8_t>>::Convert(
    const Array<uint8_t>& input) {
  COMPILE_ASSERT(sizeof(uint8_t) == sizeof(char),
                 uint8_t_same_size_as_unsigned_char);
  return input.is_null()
             ? WebString()
             : WebString::fromUTF8(
                   reinterpret_cast<const char*>(&input.front()), input.size());
}

// static
RectPtr TypeConverter<RectPtr, WebRect>::Convert(const WebRect& input) {
  RectPtr result(Rect::New());
  result->x = input.x;
  result->y = input.y;
  result->width = input.width;
  result->height = input.height;
  return result.Pass();
};

// static
Array<uint8_t> TypeConverter<Array<uint8_t>, WebString>::Convert(
    const WebString& input) {
  if (input.isNull())
    return Array<uint8_t>();
  const std::string utf8 = input.utf8();
  Array<uint8_t> result(utf8.size());
  COMPILE_ASSERT(sizeof(uint8_t) == sizeof(char),
                 uint8_t_same_size_as_unsigned_char);
  memcpy(&result.front(), utf8.data(), utf8.size());
  return result.Pass();
}

}  // namespace mojo
