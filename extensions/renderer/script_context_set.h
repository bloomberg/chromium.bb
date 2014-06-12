// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_CONTEXT_SET_H_
#define EXTENSIONS_RENDERER_SCRIPT_CONTEXT_SET_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "v8/include/v8.h"

class GURL;

namespace base {
class ListValue;
}

namespace content {
class RenderView;
}

namespace v8 {
class Context;
}

namespace extensions {
class ScriptContext;

// A container of ExtensionBindingsContext. Since calling JavaScript within a
// context can cause any number of contexts to be created or destroyed, this
// has additional smarts to help with the set changing underneath callers.
class ScriptContextSet {
 public:
  ScriptContextSet();
  ~ScriptContextSet();

  int size() const;

  // Takes ownership of |context|.
  void Add(ScriptContext* context);

  // If the specified context is contained in this set, remove it, then delete
  // it asynchronously. After this call returns the context object will still
  // be valid, but its frame() pointer will be cleared.
  void Remove(ScriptContext* context);

  // Returns a copy to protect against changes.
  typedef std::set<ScriptContext*> ContextSet;
  ContextSet GetAll() const;

  // Gets the ScriptContext corresponding to v8::Context::GetCurrent(), or
  // NULL if no such context exists.
  ScriptContext* GetCurrent() const;

  // Gets the ScriptContext corresponding to v8::Context::GetCalling(), or
  // NULL if no such context exists.
  ScriptContext* GetCalling() const;

  // Gets the ScriptContext corresponding to the specified
  // v8::Context or NULL if no such context exists.
  ScriptContext* GetByV8Context(v8::Handle<v8::Context> context) const;

  // Synchronously runs |callback| with each ScriptContext that belongs to
  // |extension_id| in |render_view|.
  //
  // An empty |extension_id| will match all extensions, and a NULL |render_view|
  // will match all render views, but try to use the inline variants of these
  // methods instead.
  void ForEach(const std::string& extension_id,
               content::RenderView* render_view,
               const base::Callback<void(ScriptContext*)>& callback) const;
  // ForEach which matches all extensions.
  void ForEach(content::RenderView* render_view,
               const base::Callback<void(ScriptContext*)>& callback) const {
    ForEach("", render_view, callback);
  }
  // ForEach which matches all render views.
  void ForEach(const std::string& extension_id,
               const base::Callback<void(ScriptContext*)>& callback) const {
    ForEach(extension_id, NULL, callback);
  }

  // Cleans up contexts belonging to an unloaded extension.
  //
  // Returns the set of ScriptContexts that were removed as a result. These
  // are safe to interact with until the end of the current event loop, since
  // they're deleted asynchronously.
  ContextSet OnExtensionUnloaded(const std::string& extension_id);

 private:
  ContextSet contexts_;

  DISALLOW_COPY_AND_ASSIGN(ScriptContextSet);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_CONTEXT_SET_H_
