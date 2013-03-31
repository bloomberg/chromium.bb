// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_H_

#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/strings/string_piece.h"
#include "chrome/renderer/extensions/chrome_v8_extension_handler.h"
#include "chrome/renderer/extensions/object_backed_native_handler.h"
#include "v8/include/v8.h"

#include <map>
#include <set>
#include <string>


namespace content {
class RenderView;
}

namespace extensions {
class ChromeV8Context;
class Dispatcher;
class Extension;

// DEPRECATED.
//
// This is a base class for chrome extension bindings.  Common features that
// are shared by different modules go here.
//
// TODO(kalman): Delete this class entirely, it has no value anymore.
//               Custom bindings should extend ObjectBackedNativeHandler.
class ChromeV8Extension : public ObjectBackedNativeHandler {
 public:
  ChromeV8Extension(Dispatcher* dispatcher, v8::Handle<v8::Context> context);
  virtual ~ChromeV8Extension();

  Dispatcher* dispatcher() { return dispatcher_; }

  ChromeV8Context* GetContext();

  // Shortcuts through to the context's render view and extension.
  content::RenderView* GetRenderView();
  const Extension* GetExtensionForRenderView();

 protected:
  Dispatcher* dispatcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeV8Extension);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_H_
