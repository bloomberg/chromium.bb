// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_V8_STRING_CONVERSIONS_H_
#define CONTENT_PUBLIC_RENDERER_V8_STRING_CONVERSIONS_H_

#include <string>
#include "base/string16.h"
#include "content/common/content_export.h"
#include "v8/include/v8.h"

namespace content {

// Converts a V8 value to a string16. Assumes the value is a string, will DCHECK
// if thats not the case.
CONTENT_EXPORT string16 V8ValueToUTF16(v8::Handle<v8::Value> v);

// Converts a V8 value to a UTF8 string. Assumes the value is a string, will
// DCHECK if thats not the case.
CONTENT_EXPORT std::string V8ValueToUTF8(v8::Handle<v8::Value> v);

// Converts string16 to V8 String.
CONTENT_EXPORT v8::Handle<v8::String> UTF16ToV8String(const string16& s);

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_V8_STRING_CONVERSIONS_H_
