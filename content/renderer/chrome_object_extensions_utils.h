// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CHROME_OBJECT_EXTENSIONS_UTILS_H_
#define CONTENT_RENDERER_CHROME_OBJECT_EXTENSIONS_UTILS_H_

#include "v8/include/v8.h"

namespace content {

v8::Handle<v8::Object> GetOrCreateChromeObject(
    v8::Isolate* isolate, v8::Handle<v8::Object> global);

}  // namespace content

#endif  // CONTENT_RENDERER_CHROME_OBJECT_EXTENSIONS_UTILS_H_
