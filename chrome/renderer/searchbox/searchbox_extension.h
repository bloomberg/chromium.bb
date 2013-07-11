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

  // Returns true if a page supports Instant, that is, if it has bound an
  // onsubmit() handler.
  static bool PageSupportsInstant(WebKit::WebFrame* frame);

  // Helpers to dispatch Javascript events.
  static void DispatchFocusChange(WebKit::WebFrame* frame);
  static void DispatchInputCancel(WebKit::WebFrame* frame);
  static void DispatchInputStart(WebKit::WebFrame* frame);
  static void DispatchKeyCaptureChange(WebKit::WebFrame* frame);
  static void DispatchMarginChange(WebKit::WebFrame* frame);
  static void DispatchMostVisitedChanged(WebKit::WebFrame* frame);
  static void DispatchSubmit(WebKit::WebFrame* frame);
  static void DispatchThemeChange(WebKit::WebFrame* frame);
  static void DispatchToggleVoiceSearch(WebKit::WebFrame* frame);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SearchBoxExtension);
};

}  // namespace extensions_v8

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCHBOX_EXTENSION_H_
