// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_BINDINGS_CONTEXT_SET_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_BINDINGS_CONTEXT_SET_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "v8/include/v8.h"

class ExtensionBindingsContext;
class GURL;
class MessageLoop;
class RenderView;

namespace base {
class ListValue;
}

namespace v8 {
class Context;
}

// A container of ExtensionBindingsContext. Since calling JavaScript within a
// context can cause any number of contexts to be created or destroyed, this
// has additional smarts to help with the set changing underneath callers.
//
// TODO(aa): Remove extension-specific bits, rename to BindingsContextSet, and
// move into renderer/bindings with DEPS to protect against dependencies on
// extensions.
class ExtensionBindingsContextSet {
 public:
  // For testing.
  static void SetDeleteLoop(MessageLoop* message_loop);

  ExtensionBindingsContextSet();
  ~ExtensionBindingsContextSet();

  int size() const;

  // Takes ownership of |context|.
  void Add(ExtensionBindingsContext* context);

  // If the specified context is contained in this set, remove it, then delete
  // it asynchronously. After this call returns the context object will still
  // be valid, but its frame() pointer will be cleared.
  void Remove(ExtensionBindingsContext* context);
  void RemoveByV8Context(v8::Handle<v8::Context> context);

  // Returns a copy to protect against changes.
  typedef std::set<ExtensionBindingsContext*> ContextSet;
  ContextSet GetAll() const;

  // Gets the ExtensionBindingsContext corresponding to the v8::Context that is
  // on the top of the stack, or NULL if no such context exists.
  ExtensionBindingsContext* GetCurrent() const;

  // Gets the ExtensionBindingsContext corresponding to the specified
  // v8::Context or NULL if no such context exists.
  ExtensionBindingsContext* GetByV8Context(
      v8::Handle<v8::Context> context) const;

  // Calls chromeHidden.<methodName> in each context for <extension_id>. If
  // render_view is non-NULL, only call the function in contexts belonging to
  // that view. The called javascript function should not return a value other
  // than v8::Undefined(). A DCHECK is setup to break if it is otherwise.
  void DispatchChromeHiddenMethod(const std::string& extension_id,
                                  const std::string& method_name,
                                  const base::ListValue& arguments,
                                  RenderView* render_view,
                                  const GURL& event_url) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionBindingsContextSet);

  // The loop we will delete ExtensionBindingsContext on. If NULL, we use
  // RenderThread's loop instead.
  static MessageLoop* delete_loop_;

  ContextSet contexts_;
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_BINDINGS_CONTEXT_SET_H_
