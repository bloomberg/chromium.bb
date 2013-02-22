// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_SEARCHBOX_EXTENSION_H_
#define CHROME_RENDERER_SEARCHBOX_SEARCHBOX_EXTENSION_H_

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

  // Extended API.
  static void DispatchOnWindowReady(WebKit::WebFrame* frame);
  static void DispatchAutocompleteResults(WebKit::WebFrame* frame);
  static void DispatchUpOrDownKeyPress(WebKit::WebFrame* frame, int count);
  static void DispatchEscKeyPress(WebKit::WebFrame* frame);
  static void DispatchKeyCaptureChange(WebKit::WebFrame* frame);
  static void DispatchMarginChange(WebKit::WebFrame* frame);
  static void DispatchThemeChange(WebKit::WebFrame* frame);

  // New Tab Page API.
  static void DispatchMostVisitedChanged(WebKit::WebFrame* frame);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SearchBoxExtension);
};

}  // namespace extensions_v8

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCHBOX_EXTENSION_H_
