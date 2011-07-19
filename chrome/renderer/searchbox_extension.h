// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_EXTENSION_H_
#define CHROME_RENDERER_SEARCHBOX_EXTENSION_H_
#pragma once

#include "base/basictypes.h"

namespace v8 {
class Extension;
}

namespace WebKit {
class WebFrame;
}

namespace extensions_v8 {

// Reference implementation of the SearchBox API as described in:
// http://dev.chromium.org/searchbox
class SearchBoxExtension {
 public:
  // Returns the v8::Extension object handling searchbox bindings. Returns null
  // if match-preview is not enabled. Caller takes ownership of returned object.
  static v8::Extension* Get();

  static void DispatchChange(WebKit::WebFrame* frame);
  static void DispatchSubmit(WebKit::WebFrame* frame);
  static void DispatchCancel(WebKit::WebFrame* frame);
  static void DispatchResize(WebKit::WebFrame* frame);

  static bool PageSupportsInstant(WebKit::WebFrame* frame);

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchBoxExtension);
};

}  // namespace extensions_v8

#endif  // CHROME_RENDERER_SEARCHBOX_EXTENSION_H_
