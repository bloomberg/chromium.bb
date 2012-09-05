// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/v8_string_conversions.h"
#include "base/utf_string_conversions.h"

namespace content {

string16 V8ValueToUTF16(v8::Handle<v8::Value> v) {
  v8::String::Value s(v);
  return string16(reinterpret_cast<const char16*>(*s), s.length());
}

std::string V8ValueToUTF8(v8::Handle<v8::Value> v) {
  return UTF16ToUTF8(V8ValueToUTF16(v));
}

v8::Handle<v8::String> UTF16ToV8String(const string16& s) {
  return v8::String::New(reinterpret_cast<const uint16_t*>(s.data()), s.size());
}

}  // namespace content
