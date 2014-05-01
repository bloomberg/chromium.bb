// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/default_dispatcher_delegate.h"

#include "extensions/renderer/script_context.h"

namespace extensions {

DefaultDispatcherDelegate::DefaultDispatcherDelegate() {
}

DefaultDispatcherDelegate::~DefaultDispatcherDelegate() {
}

// DispatcherDelegate implementation.
scoped_ptr<ScriptContext> DefaultDispatcherDelegate::CreateScriptContext(
    const v8::Handle<v8::Context>& v8_context,
    blink::WebFrame* frame,
    const Extension* extension,
    Feature::Context context_type) {
  return make_scoped_ptr(
      new ScriptContext(v8_context, frame, extension, context_type));
}

}  // namespace extensions
