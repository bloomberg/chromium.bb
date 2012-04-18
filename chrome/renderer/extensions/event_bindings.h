// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EVENT_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_EVENT_BINDINGS_H_
#pragma once

#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "v8/include/v8.h"

class ChromeV8Extension;
class ExtensionDispatcher;

namespace v8 {
class Extension;
}

// This class deals with the javascript bindings related to Event objects.
class EventBindings : public ChromeV8Extension {
 public:
  explicit EventBindings(ExtensionDispatcher* dispatcher);

 private:
  v8::Handle<v8::Value> AttachEvent(const v8::Arguments& args);
  v8::Handle<v8::Value> DetachEvent(const v8::Arguments& args);

  bool IsLazyBackgroundPage(const Extension* extension);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EVENT_BINDINGS_H_
