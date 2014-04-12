// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_RENDER_VIEW_OBSERVER_NATIVES_H_
#define CHROME_RENDERER_EXTENSIONS_RENDER_VIEW_OBSERVER_NATIVES_H_

#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "v8/include/v8.h"

namespace extensions {
class Dispatcher;

// Native functions for JS to run callbacks upon RenderView events.
class RenderViewObserverNatives : public ChromeV8Extension {
 public:
  RenderViewObserverNatives(Dispatcher* dispatcher, ChromeV8Context* context);

 private:
  // Runs a callback upon creation of new document element inside a render view
  // (document.documentElement).
  void OnDocumentElementCreated(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  DISALLOW_COPY_AND_ASSIGN(RenderViewObserverNatives);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_RENDER_VIEW_OBSERVER_NATIVES_H_
