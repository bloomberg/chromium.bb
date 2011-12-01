// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_V8_CONTEXT_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_V8_CONTEXT_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "v8/include/v8.h"

namespace WebKit {
class WebFrame;
}

namespace content {
class RenderView;
}

// Chrome's wrapper for a v8 context.
//
// TODO(aa): Consider converting this back to a set of bindings_utils. It would
// require adding WebFrame::GetIsolatedWorldIdByV8Context() to WebCore, but then
// we won't need this object and it's a bit less state to keep track of.
class ChromeV8Context {
 public:
  ChromeV8Context(v8::Handle<v8::Context> context,
                  WebKit::WebFrame* frame,
                  const std::string& extension_id);
  ~ChromeV8Context();

  v8::Handle<v8::Context> v8_context() const {
    return v8_context_;
  }

  const std::string& extension_id() const {
    return extension_id_;
  }

  WebKit::WebFrame* web_frame() const {
    return web_frame_;
  }
  void clear_web_frame() {
    web_frame_ = NULL;
  }

  // Returns a special Chrome-specific hidden object that is associated with a
  // context, but not reachable from the JavaScript in that context. This is
  // used by our v8::Extension implementations as a way to share code and as a
  // bridge between C++ and JavaScript.
  static v8::Handle<v8::Value> GetOrCreateChromeHidden(
      v8::Handle<v8::Context> context);

  // Return the chromeHidden object associated with this context, or an empty
  // handle if no chrome hidden has been created (by GetOrCreateChromeHidden)
  // yet for this context.
  v8::Handle<v8::Value> GetChromeHidden() const;

  // Returns the RenderView associated with this context. Can return NULL if the
  // context is in the process of being destroyed.
  content::RenderView* GetRenderView() const;

  // Fires the onload and onunload events on the chromeHidden object.
  // TODO(aa): Move this to EventBindings.
  void DispatchOnLoadEvent(bool is_extension_process,
                           bool is_incognito_process) const;
  void DispatchOnUnloadEvent() const;

  // Call the named method of the chromeHidden object in this context.
  // The function can be a sub-property like "Port.dispatchOnMessage". Returns
  // the result of the function call in |result| if |result| is non-NULL. If the
  // named method does not exist, returns false.
  bool CallChromeHiddenMethod(
      const std::string& function_name,
      int argc,
      v8::Handle<v8::Value>* argv,
      v8::Handle<v8::Value>* result) const;

 private:
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

  DISALLOW_COPY_AND_ASSIGN(ChromeV8Context);
};

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_V8_CONTEXT_H_
