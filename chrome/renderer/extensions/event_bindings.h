// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EVENT_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_EVENT_BINDINGS_H_

#include "v8/include/v8.h"

namespace extensions {
class ChromeV8Extension;
class Dispatcher;
class EventFilter;

// This class deals with the javascript bindings related to Event objects.
class EventBindings {
 public:
  static ChromeV8Extension* Create(Dispatcher* dispatcher,
                                   v8::Handle<v8::Context> context);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_EVENT_BINDINGS_H_
