// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_
#define CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_
#pragma once

#include "base/memory/linked_ptr.h"
#include "base/memory/singleton.h"
#include "base/string_piece.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

#include <list>
#include <string>

class Extension;
class ExtensionDispatcher;
class RenderView;

namespace WebKit {
class WebFrame;
}

namespace bindings_utils {

// This is a base class for chrome extension bindings.  Common features that
// are shared by different modules go here.
class ExtensionBase : public v8::Extension {
 public:
  ExtensionBase(const char* name,
                const char* source,
                int dep_count,
                const char** deps,
                ExtensionDispatcher* extension_dispatcher)
      : v8::Extension(name, source, dep_count, deps),
        extension_dispatcher_(extension_dispatcher) {
  }

  // Derived classes should call this at the end of their implementation in
  // order to expose common native functions, like GetChromeHidden, to the
  // v8 extension.
  virtual v8::Handle<v8::FunctionTemplate>
      GetNativeFunction(v8::Handle<v8::String> name);

  // TODO(jstritar): Used for testing http://crbug.com/91582. Remove when done.
  ExtensionDispatcher* extension_dispatcher() { return extension_dispatcher_; }

 protected:
  template<class T>
  static T* GetFromArguments(const v8::Arguments& args) {
    CHECK(!args.Data().IsEmpty());
    T* result = static_cast<T*>(args.Data().As<v8::External>()->Value());
    return result;
  }

  // Note: do not call this function before or during the chromeHidden.onLoad
  // event dispatch. The URL might not have been committed yet and might not
  // be an extension URL.
  const ::Extension* GetExtensionForCurrentContext() const;

  // Checks that the current context contains an extension that has permission
  // to execute the specified function. If it does not, a v8 exception is thrown
  // and the method returns false. Otherwise returns true.
  bool CheckPermissionForCurrentContext(const std::string& function_name) const;

  // Returns a hidden variable for use by the bindings that is unreachable
  // by the page.
  static v8::Handle<v8::Value> GetChromeHidden(const v8::Arguments& args);

  ExtensionDispatcher* extension_dispatcher_;

 private:
  // Helper to print from bindings javascript.
  static v8::Handle<v8::Value> Print(const v8::Arguments& args);
};

const char* GetStringResource(int resource_id);

// Contains information about a JavaScript context that is hosting extension
// bindings.
struct ContextInfo {
  ContextInfo(v8::Persistent<v8::Context> context,
              const std::string& extension_id,
              WebKit::WebFrame* frame);
  ~ContextInfo();

  v8::Persistent<v8::Context> context;

  // The extension ID this context is associated with.
  std::string extension_id;

  // The frame the context is associated with. ContextInfo can outlive its
  // frame, so this should not be dereferenced. It is stored only for use for
  // comparison.
  void* unsafe_frame;

  // A count of the number of events that are listening in this context. When
  // this is zero, |context| will be a weak handle.
  int num_connected_events;
};
typedef std::list< linked_ptr<ContextInfo> > ContextList;

// Returns a mutable reference to the ContextList. Note: be careful using this.
// Calling into javascript may result in the list being modified, so don't rely
// on iterators remaining valid between calls to javascript.
ContextList& GetContexts();

// Returns the ContextInfo item that has the given context.
ContextList::iterator FindContext(v8::Handle<v8::Context> context);

// Returns the ContextInfo for the current v8 context.
ContextInfo* GetInfoForCurrentContext();

// Contains info relevant to a pending API request.
struct PendingRequest {
 public :
  PendingRequest(v8::Persistent<v8::Context> context, const std::string& name);
  ~PendingRequest();

  v8::Persistent<v8::Context> context;
  std::string name;
};
typedef std::map<int, linked_ptr<PendingRequest> > PendingRequestMap;

// Returns a mutable reference to the PendingRequestMap.
PendingRequestMap& GetPendingRequestMap();

// Returns the current RenderView, based on which V8 context is current.  It is
// an error to call this when not in a V8 context.
RenderView* GetRenderViewForCurrentContext();

// Call the named javascript function with the given arguments in a context.
// The function name should be reachable from the chromeHidden object, and can
// be a sub-property like "Port.dispatchOnMessage". Returns the result of
// the function call. If an exception is thrown an empty Handle will be
// returned.
v8::Handle<v8::Value> CallFunctionInContext(v8::Handle<v8::Context> context,
    const std::string& function_name, int argc,
    v8::Handle<v8::Value>* argv);

}  // namespace bindings_utils

#endif  // CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_
