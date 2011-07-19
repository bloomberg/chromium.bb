// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCH_EXTENSION_H_
#define CHROME_RENDERER_SEARCH_EXTENSION_H_
#pragma once

namespace v8 {
class Extension;
}

namespace extensions_v8 {

class SearchExtension {
 public:
  // Returns the v8::Extension object handling search bindings. Returns null if
  // match-preview is not enabled. Caller takes ownership of returned object.
  static v8::Extension* Get();
};

}  // namespace extensions_v8

#endif  // CHROME_RENDERER_SEARCH_EXTENSION_H_
