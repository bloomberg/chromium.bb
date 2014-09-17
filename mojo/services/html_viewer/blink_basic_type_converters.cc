// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/blink_basic_type_converters.h"

#include "mojo/public/cpp/bindings/string.h"
#include "third_party/WebKit/public/platform/WebString.h"

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
Array<uint8_t> TypeConverter<Array<uint8_t>, blink::WebString>::Convert(
    const blink::WebString& input) {
  std::string utf8 = input.utf8();
  Array<uint8_t> result(utf8.size());
  for (size_t i = 0; i < utf8.size(); ++i)
    result[i] = utf8[i];
  return result.Pass();
}

}  // namespace mojo
