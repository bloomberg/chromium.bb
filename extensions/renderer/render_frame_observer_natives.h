// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_RENDER_FRAME_OBSERVER_NATIVES_H_
#define EXTENSIONS_RENDERER_RENDER_FRAME_OBSERVER_NATIVES_H_

#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {
class ScriptContext;

// Native functions for JS to run callbacks upon RenderFrame events.
class RenderFrameObserverNatives : public ObjectBackedNativeHandler {
 public:
  explicit RenderFrameObserverNatives(ScriptContext* context);

 private:
  // Runs a callback upon creation of new document element inside a render frame
  // (document.documentElement).
  void OnDocumentElementCreated(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  DISALLOW_COPY_AND_ASSIGN(RenderFrameObserverNatives);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_RENDER_FRAME_OBSERVER_NATIVES_H_
