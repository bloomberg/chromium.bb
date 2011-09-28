// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_BINDINGS_CONTEXT_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_BINDINGS_CONTEXT_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/task.h"
#include "v8/include/v8.h"

namespace base {
class ListValue;
}

namespace WebKit {
class WebFrame;
}

class ExtensionDispatcher;
class GURL;
class MessageLoop;
class RenderView;

// A v8 context that contains extension bindings.
class ExtensionBindingsContext {
 public:
  typedef std::vector<ExtensionBindingsContext*> ContextList;

  static void HandleV8ContextCreated(WebKit::WebFrame* frame,
                                     v8::Handle<v8::Context> v8_context,
                                     ExtensionDispatcher* extension_dispatcher,
                                     int isolated_world_id);

  static void HandleV8ContextReleased(WebKit::WebFrame* frame,
                                      v8::Handle<v8::Context> v8_context,
                                      int isolated_world_id);

  static ExtensionBindingsContext* GetCurrent();

  static ExtensionBindingsContext* GetByV8Context(
      const v8::Handle<v8::Context>& v8_context);

  static ContextList GetAll();

  // Calls chromeHidden.<methodName> in each context for <extension_id>. If
  // render_view is non-NULL, only call the function in contexts belonging to
  // that view. The called javascript function should not return a value other
  // than v8::Undefined(). A DCHECK is setup to break if it is otherwise.
  static void DispatchChromeHiddenMethod(const std::string& extension_id,
                                         const std::string& method_name,
                                         const base::ListValue& arguments,
                                         RenderView* render_view,
                                         const GURL& event_url);

  // TODO(aa): Remove this. See crbug.com/98375.
  static void SetRenderThreadMessageLoop(MessageLoop* loop);

  static void SetTestExtensionId(const std::string&);

  v8::Handle<v8::Context> v8_context() const {
    return v8_context_;
  }

  const std::string& extension_id() const {
    return extension_id_;
  }

  // Returns the RenderView associated with this context. Can return NULL if the
  // context is in the process of being destroyed.
  RenderView* GetRenderView() const;

  // Call the named method of the chromeHidden object in this context.
  // The function can be a sub-property like "Port.dispatchOnMessage". Returns
  // the result of the function call. If an exception is thrown an empty Handle
  // will be returned.
  v8::Handle<v8::Value> CallChromeHiddenMethod(const std::string& function_name,
                                               int argc,
                                               v8::Handle<v8::Value>* argv);

 private:
  // For ~ExtensionBindingsContext().
  friend void DeleteTask<ExtensionBindingsContext>::Run();

  static MessageLoop* GetRenderThreadMessageLoop();

  ExtensionBindingsContext(v8::Handle<v8::Context> context,
                           WebKit::WebFrame* frame,
                           const std::string& extension_id);

  ~ExtensionBindingsContext();

  static MessageLoop* render_thread_message_loop_;

  // The v8 context the bindings are accessible to. We keep a strong reference
  // to it for simplicity. In the case of content scripts, this is necessary
  // because we want all scripts from the same extension for the same frame to
  // run in the same context, so we can't have the contexts being GC'd if
  // nothing is happening. In the case of page contexts, this isn't necessary
  // since the DOM keeps the context alive, but it makes things simpler to not
  // distinguish the two cases.
  v8::Persistent<v8::Context> v8_context_;

  // The WebFrame associated with this context. This can be NULL because this
  // object can outlive is destroyed asynchronously.
  WebKit::WebFrame* web_frame_;

  // The extension ID this context is associated with.
  std::string extension_id_;
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_BINDINGS_CONTEXT_H_
