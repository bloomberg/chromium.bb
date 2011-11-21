// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_H_
#pragma once

#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "chrome/renderer/extensions/chrome_v8_extension_handler.h"
#include "v8/include/v8.h"

#include <map>
#include <set>
#include <string>

class ChromeV8Context;
class Extension;
class ExtensionDispatcher;

namespace content {
class RenderView;
}

// This is a base class for chrome extension bindings.  Common features that
// are shared by different modules go here.
//
// TODO(aa): Remove the extension-system specific bits of this and move to
// renderer/, or even to renderer/bindings and use DEPS to enforce separation
// from extension system.
//
// TODO(aa): Add unit testing for this class.
class ChromeV8Extension : public v8::Extension {
 public:
  typedef std::set<ChromeV8Extension*> InstanceSet;
  static const InstanceSet& GetAll();

  ChromeV8Extension(const char* name,
                    int resource_id,
                    ExtensionDispatcher* extension_dispatcher);
  ChromeV8Extension(const char* name,
                    int resource_id,
                    int dependency_count,
                    const char** dependencies,
                    ExtensionDispatcher* extension_dispatcher);
  virtual ~ChromeV8Extension();

  ExtensionDispatcher* extension_dispatcher() { return extension_dispatcher_; }

  // Returns a hidden variable for use by the bindings in the specified context
  // that is unreachable by the page for the current context.
  static v8::Handle<v8::Value> GetChromeHidden(
      const v8::Handle<v8::Context>& context);

  void ContextWillBeReleased(ChromeV8Context* context);

  // Derived classes should call this at the end of their implementation in
  // order to expose common native functions, like GetChromeHidden, to the
  // v8 extension.
  virtual v8::Handle<v8::FunctionTemplate>
      GetNativeFunction(v8::Handle<v8::String> name) OVERRIDE;

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
  const ::Extension* GetExtensionForCurrentRenderView() const;

  // Checks that the current context contains an extension that has permission
  // to execute the specified function. If it does not, a v8 exception is thrown
  // and the method returns false. Otherwise returns true.
  bool CheckCurrentContextAccessToExtensionAPI(
      const std::string& function_name) const;

  // Create a handler for |context|. If a subclass of ChromeV8Extension wishes
  // to support handlers, it should override this.
  virtual ChromeV8ExtensionHandler* CreateHandler(ChromeV8Context* context);

  // Returns the chromeHidden object for the current context.
  static v8::Handle<v8::Value> GetChromeHidden(const v8::Arguments& args);

  ExtensionDispatcher* extension_dispatcher_;

 private:
  static base::StringPiece GetStringResource(int resource_id);

  // Helper to print from bindings javascript.
  static v8::Handle<v8::Value> Print(const v8::Arguments& args);

  // Handle a native function call from JavaScript.
  static v8::Handle<v8::Value> HandleNativeFunction(const v8::Arguments& args);

  // Get the handler instance for |context|, or NULL if no such context exists.
  ChromeV8ExtensionHandler* GetHandler(ChromeV8Context* context) const;

  // Map of all current handlers.
  typedef std::map<ChromeV8Context*,
                   linked_ptr<ChromeV8ExtensionHandler> > HandlerMap;
  HandlerMap handlers_;

  DISALLOW_COPY_AND_ASSIGN(ChromeV8Extension);
};

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_H_
