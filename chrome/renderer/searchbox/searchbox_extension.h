// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_SEARCHBOX_EXTENSION_H_
#define CHROME_RENDERER_SEARCHBOX_SEARCHBOX_EXTENSION_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"

namespace v8 {
class Extension;
}

namespace blink {
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
  static bool PageSupportsInstant(blink::WebFrame* frame);

  // Helpers to dispatch Javascript events.
  static void DispatchChromeIdentityCheckResult(blink::WebFrame* frame,
                                                const base::string16& identity,
                                                bool identity_match);
  static void DispatchFocusChange(blink::WebFrame* frame);
  static void DispatchInputCancel(blink::WebFrame* frame);
  static void DispatchInputStart(blink::WebFrame* frame);
  static void DispatchKeyCaptureChange(blink::WebFrame* frame);
  static void DispatchMarginChange(blink::WebFrame* frame);
  static void DispatchMostVisitedChanged(blink::WebFrame* frame);
  static void DispatchSubmit(blink::WebFrame* frame);
  static void DispatchSuggestionChange(blink::WebFrame* frame);
  static void DispatchThemeChange(blink::WebFrame* frame);
  static void DispatchToggleVoiceSearch(blink::WebFrame* frame);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SearchBoxExtension);
};

}  // namespace extensions_v8

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCHBOX_EXTENSION_H_
