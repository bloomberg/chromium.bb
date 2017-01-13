// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_BINDING_TYPES_H_
#define EXTENSIONS_RENDERER_API_BINDING_TYPES_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "v8/include/v8.h"

namespace extensions {
namespace binding {

// Types of changes for event listener registration.
enum class EventListenersChanged {
  HAS_LISTENERS,  // The event had no listeners, and now does.
  NO_LISTENERS,   // The event had listeners, and now does not.
};

// A callback to execute the given v8::Function with the provided context and
// arguments.
using RunJSFunction = base::Callback<void(v8::Local<v8::Function>,
                                          v8::Local<v8::Context>,
                                          int argc,
                                          v8::Local<v8::Value>[])>;

// A callback to execute the given v8::Function synchronously and return the
// result. Note that script can be suspended, so you need to be certain that
// it is not before expected a synchronous result. We use a Global instead of a
// Local because certain implementations need to create a persistent handle in
// order to prevent immediate destruction of the locals.
// TODO(devlin): if we could, using Local here with an EscapableHandleScope
// would be preferable.
using RunJSFunctionSync =
    base::Callback<v8::Global<v8::Value>(v8::Local<v8::Function>,
                                         v8::Local<v8::Context>,
                                         int argc,
                                         v8::Local<v8::Value>[])>;

}  // namespace binding
}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_BINDING_TYPES_H_
