// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_PAGE_CAPTURE_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_PAGE_CAPTURE_CUSTOM_BINDINGS_H_
#pragma once

#include "chrome/renderer/extensions/chrome_v8_extension.h"

namespace extensions {

// Implements custom bindings for the pageCapture API.
class PageCaptureCustomBindings : public ChromeV8Extension {
 public:
  PageCaptureCustomBindings(int dependency_count, const char** dependencies);

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) OVERRIDE;

 private:
  static v8::Handle<v8::Value> SendResponseAck(const v8::Arguments& args);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_PAGE_CAPTURE_CUSTOM_BINDINGS_H_
