// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_context_set.h"

#include "base/message_loop/message_loop.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8.h"

namespace extensions {

ScriptContextSet::ScriptContextSet() {
}
ScriptContextSet::~ScriptContextSet() {
}

int ScriptContextSet::size() const {
  return static_cast<int>(contexts_.size());
}

void ScriptContextSet::Add(ScriptContext* context) {
#if DCHECK_IS_ON
  // It's OK to insert the same context twice, but we should only ever have
  // one ScriptContext per v8::Context.
  for (ContextSet::iterator iter = contexts_.begin(); iter != contexts_.end();
       ++iter) {
    ScriptContext* candidate = *iter;
    if (candidate != context)
      DCHECK(candidate->v8_context() != context->v8_context());
  }
#endif
  contexts_.insert(context);
}

void ScriptContextSet::Remove(ScriptContext* context) {
  if (contexts_.erase(context)) {
    context->Invalidate();
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, context);
  }
}

ScriptContextSet::ContextSet ScriptContextSet::GetAll() const {
  return contexts_;
}

ScriptContext* ScriptContextSet::GetCurrent() const {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  return isolate->InContext() ? GetByV8Context(isolate->GetCurrentContext())
                              : NULL;
}

ScriptContext* ScriptContextSet::GetCalling() const {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> calling = isolate->GetCallingContext();
  return calling.IsEmpty() ? NULL : GetByV8Context(calling);
}

ScriptContext* ScriptContextSet::GetByV8Context(
    v8::Handle<v8::Context> v8_context) const {
  for (ContextSet::const_iterator iter = contexts_.begin();
       iter != contexts_.end();
       ++iter) {
    if ((*iter)->v8_context() == v8_context)
      return *iter;
  }

  return NULL;
}

void ScriptContextSet::ForEach(
    const std::string& extension_id,
    content::RenderView* render_view,
    const base::Callback<void(ScriptContext*)>& callback) const {
  // We copy the context list, because calling into javascript may modify it
  // out from under us.
  ContextSet contexts = GetAll();

  for (ContextSet::iterator it = contexts.begin(); it != contexts.end(); ++it) {
    ScriptContext* context = *it;

    // For the same reason as above, contexts may become invalid while we run.
    if (!context->is_valid())
      continue;

    if (!extension_id.empty()) {
      const Extension* extension = context->extension();
      if (!extension || (extension_id != extension->id()))
        continue;
    }

    content::RenderView* context_render_view = context->GetRenderView();
    if (!context_render_view)
      continue;

    if (render_view && render_view != context_render_view)
      continue;

    callback.Run(context);
  }
}

ScriptContextSet::ContextSet ScriptContextSet::OnExtensionUnloaded(
    const std::string& extension_id) {
  ContextSet contexts = GetAll();
  ContextSet removed;

  // Clean up contexts belonging to the unloaded extension. This is done so
  // that content scripts (which remain injected into the page) don't continue
  // receiving events and sending messages.
  for (ContextSet::iterator it = contexts.begin(); it != contexts.end(); ++it) {
    if ((*it)->extension() && (*it)->extension()->id() == extension_id) {
      (*it)->DispatchOnUnloadEvent();
      removed.insert(*it);
      Remove(*it);
    }
  }

  return removed;
}

}  // namespace extensions
