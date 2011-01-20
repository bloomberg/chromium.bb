// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_
#define CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_
#pragma once

#include "base/linked_ptr.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

#include <list>
#include <string>

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
                const char** deps)
      : v8::Extension(name, source, dep_count, deps) {}

  // Note: do not call this function before or during the chromeHidden.onLoad
  // event dispatch. The URL might not have been committed yet and might not
  // be an extension URL.
  static std::string ExtensionIdForCurrentContext();

  // Derived classes should call this at the end of their implementation in
  // order to expose common native functions, like GetChromeHidden, to the
  // v8 extension.
  virtual v8::Handle<v8::FunctionTemplate>
      GetNativeFunction(v8::Handle<v8::String> name);

 protected:
  // Returns a hidden variable for use by the bindings that is unreachable
  // by the page.
  static v8::Handle<v8::Value> GetChromeHidden(const v8::Arguments& args);
};

const char* GetStringResource(int resource_id);

// Contains information about a single javascript context.
struct ContextInfo {
  ContextInfo(v8::Persistent<v8::Context> context,
              const std::string& extension_id,
              WebKit::WebFrame* parent_frame,
              RenderView* render_view);
  ~ContextInfo();

  v8::Persistent<v8::Context> context;
  std::string extension_id;  // empty if the context is not an extension

  // If this context is a content script, parent will be the frame that it
  // was injected in.  This is NULL if the context is not a content script.
  WebKit::WebFrame* parent_frame;

  // The RenderView that this context belongs to.  This is not guaranteed to be
  // a valid pointer, and is used for comparisons only.  Do not dereference.
  RenderView* render_view;

  // A count of the number of events that are listening in this context. When
  // this is zero, |context| will be a weak handle.
  int num_connected_events;
};
typedef std::list< linked_ptr<ContextInfo> > ContextList;

// Returns a mutable reference to the ContextList. Note: be careful using this.
// Calling into javascript may result in the list being modified, so don't rely
// on iterators remaining valid between calls to javascript.
ContextList& GetContexts();

// Returns a (copied) list of contexts that have the given extension_id.
ContextList GetContextsForExtension(const std::string& extension_id);

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
