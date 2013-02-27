// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_H_

#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/string_piece.h"
#include "chrome/renderer/extensions/chrome_v8_extension_handler.h"
#include "chrome/renderer/extensions/native_handler.h"
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

// This is a base class for chrome extension bindings.  Common features that
// are shared by different modules go here.
// TODO(koz): Rename this to ExtensionNativeModule.
class ChromeV8Extension : public NativeHandler {
 public:
  typedef std::set<ChromeV8Extension*> InstanceSet;
  static const InstanceSet& GetAll();

  explicit ChromeV8Extension(Dispatcher* dispatcher);
  virtual ~ChromeV8Extension();

  Dispatcher* dispatcher() { return dispatcher_; }

 protected:
  template<class T>
  static T* GetFromArguments(const v8::Arguments& args) {
    CHECK(!args.Data().IsEmpty());
    T* result = static_cast<T*>(args.Data().As<v8::External>()->Value());
    return result;
  }

  // Gets the render view for the current v8 context.
  static content::RenderView* GetCurrentRenderView();

  // Note: do not call this function before or during the chromeHidden.onLoad
  // event dispatch. The URL might not have been committed yet and might not
  // be an extension URL.
  const Extension* GetExtensionForCurrentRenderView() const;

  // Returns the chromeHidden object for the current context.
  static v8::Handle<v8::Value> GetChromeHidden(const v8::Arguments& args);

  Dispatcher* dispatcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeV8Extension);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_H_
