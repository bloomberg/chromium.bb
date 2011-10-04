// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_BINDINGS_CONTEXT_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_BINDINGS_CONTEXT_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "v8/include/v8.h"

namespace base {
class ListValue;
}

namespace WebKit {
class WebFrame;
}

class RenderView;

// A v8 context that contains extension bindings.
//
// TODO(aa): Remove the extension-specific bits from this, rename
// BindingsContext, and move to renderer/bindings with DEPS rules to prevent
// dependencies on extensions.
class ExtensionBindingsContext {
 public:
  ExtensionBindingsContext(v8::Handle<v8::Context> context,
                           WebKit::WebFrame* frame,
                           const std::string& extension_id);
  ~ExtensionBindingsContext();

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

  // Returns the RenderView associated with this context. Can return NULL if the
  // context is in the process of being destroyed.
  RenderView* GetRenderView() const;

  // Fires the onload event on the chromeHidden object.
  void FireOnLoadEvent(bool is_extension_process,
                       bool is_incognito_process) const;

  // Call the named method of the chromeHidden object in this context.
  // The function can be a sub-property like "Port.dispatchOnMessage". Returns
  // the result of the function call. If an exception is thrown an empty Handle
  // will be returned.
  v8::Handle<v8::Value> CallChromeHiddenMethod(
      const std::string& function_name,
      int argc,
      v8::Handle<v8::Value>* argv) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionBindingsContext);

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
