// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/blink_basic_type_converters.h"

#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/string.h"

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
RectPtr TypeConverter<RectPtr, WebRect>::Convert(const WebRect& input) {
  RectPtr result(Rect::New());
  result->x = input.x;
  result->y = input.y;
  result->width = input.width;
  result->height = input.height;
  return result.Pass();
};

// static
Array<uint8_t> TypeConverter<Array<uint8_t>, blink::WebString>::Convert(
    const blink::WebString& input) {
  std::string utf8 = input.utf8();
  Array<uint8_t> result(utf8.size());
  for (size_t i = 0; i < utf8.size(); ++i)
    result[i] = utf8[i];
  return result.Pass();
}

}  // namespace mojo
