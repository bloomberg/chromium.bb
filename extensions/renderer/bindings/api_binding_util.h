// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDING_API_BINDING_UTIL_H_
#define EXTENSIONS_RENDERER_BINDING_API_BINDING_UTIL_H_

#include <string>

#include "v8/include/v8.h"

namespace extensions {
namespace binding {

// Returns true if the given |context| is considered valid. Contexts can be
// invalidated if various objects or scripts hold onto references after when
// blink releases the context, but we don't want to handle interactions past
// this point. Additionally, simply checking if gin::PerContextData exists is
// insufficient, because gin::PerContextData is released after the notifications
// for releasing the script context, and author script can run between those
// points. See https://crbug.com/772071.
bool IsContextValid(v8::Local<v8::Context> context);

// Marks the given |context| as invalid.
void InvalidateContext(v8::Local<v8::Context> context);

// Returns the string version of the current platform, one of "chromeos",
// "linux", "win", or "mac".
std::string GetPlatformString();

}  // namespace binding
}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDING_API_BINDING_UTIL_H_
