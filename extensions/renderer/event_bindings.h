// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EVENT_BINDINGS_H_
#define EXTENSIONS_RENDERER_EVENT_BINDINGS_H_

#include "v8/include/v8.h"

namespace extensions {
class Dispatcher;
class ObjectBackedNativeHandler;
class ScriptContext;

// This class deals with the javascript bindings related to Event objects.
class EventBindings {
 public:
  static ObjectBackedNativeHandler* Create(Dispatcher* dispatcher,
                                           ScriptContext* context);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EVENT_BINDINGS_H_
