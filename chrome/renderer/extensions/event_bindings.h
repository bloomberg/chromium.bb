// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EVENT_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_EVENT_BINDINGS_H_
#pragma once

#include <string>

class ExtensionDispatcher;
class GURL;
class RenderThreadBase;
class RenderView;

namespace base {
class ListValue;
}

namespace v8 {
class Context;
class Extension;
template <class T> class Handle;
}

namespace WebKit {
class WebFrame;
}

// This class deals with the javascript bindings related to Event objects.
class EventBindings {
 public:
  static const char* kName;  // The v8::Extension name, for dependencies.
  static const char* kTestingExtensionId;

  static v8::Extension* Get(ExtensionDispatcher* dispatcher);

  // Allow RenderThread to be mocked out.
  static void SetRenderThread(RenderThreadBase* thread);
  static RenderThreadBase* GetRenderThread();

  // Handle a script context coming / going away.
  static void HandleContextCreated(WebKit::WebFrame* frame,
                                   v8::Handle<v8::Context> context,
                                   ExtensionDispatcher* extension_dispatcher,
                                   int isolated_world_id);
  static void HandleContextDestroyed(WebKit::WebFrame* frame);

  // Calls the given function in each registered context which is listening for
  // events.  If render_view is non-NULL, only call the function in contexts
  // belonging to that view.  See comments on
  // bindings_utils::CallFunctionInContext for more details.
  // The called javascript function should not return a value other than
  // v8::Undefined(). A DCHECK is setup to break if it is otherwise.
  static void CallFunction(const std::string& extension_id,
                           const std::string& function_name,
                           const base::ListValue& arguments,
                           RenderView* render_view,
                           const GURL& event_url);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EVENT_BINDINGS_H_
