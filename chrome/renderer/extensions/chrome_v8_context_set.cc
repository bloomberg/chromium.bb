// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_v8_context_set.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/tracked_objects.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/constants.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"

using content::RenderThread;
using content::V8ValueConverter;

namespace extensions {

namespace {

// Returns true if the extension running in the given |render_view| has
// sufficient permissions to access the data.
//
// TODO(aa): This looks super suspicious. Is it correct? Can we use something
// else already in the system? Should it be moved elsewhere?
  bool HasSufficientPermissions(content::RenderView* render_view,
                                const GURL& event_url) {
  // During unit tests, we might be invoked without a v8 context. In these
  // cases, we only allow empty event_urls and short-circuit before retrieving
  // the render view from the current context.
  if (!event_url.is_valid())
    return true;

  WebKit::WebDocument document =
      render_view->GetWebView()->mainFrame()->document();
  return GURL(document.url()).SchemeIs(extensions::kExtensionScheme) &&
       document.securityOrigin().canRequest(event_url);
}

}  // namespace

ChromeV8ContextSet::ChromeV8ContextSet() {
}
ChromeV8ContextSet::~ChromeV8ContextSet() {
}

int ChromeV8ContextSet::size() const {
  return static_cast<int>(contexts_.size());
}

void ChromeV8ContextSet::Add(ChromeV8Context* context) {
  if (DCHECK_IS_ON()) {
    // It's OK to insert the same context twice, but we should only ever have
    // one ChromeV8Context per v8::Context.
    for (ContextSet::iterator iter = contexts_.begin(); iter != contexts_.end();
        ++iter) {
      ChromeV8Context* candidate = *iter;
      if (candidate != context)
        DCHECK(candidate->v8_context() != context->v8_context());
    }
  }
  contexts_.insert(context);
}

void ChromeV8ContextSet::Remove(ChromeV8Context* context) {
  if (contexts_.erase(context)) {
    context->clear_web_frame();
    MessageLoop::current()->DeleteSoon(FROM_HERE, context);
  }
}

ChromeV8ContextSet::ContextSet ChromeV8ContextSet::GetAll() const {
  return contexts_;
}

ChromeV8Context* ChromeV8ContextSet::GetCurrent() const {
  if (!v8::Context::InContext())
    return NULL;
  else
    return GetByV8Context(v8::Context::GetCurrent());
}

ChromeV8Context* ChromeV8ContextSet::GetByV8Context(
    v8::Handle<v8::Context> v8_context) const {
  for (ContextSet::const_iterator iter = contexts_.begin();
       iter != contexts_.end(); ++iter) {
    if ((*iter)->v8_context() == v8_context)
      return *iter;
  }

  return NULL;
}

void ChromeV8ContextSet::DispatchChromeHiddenMethod(
    const std::string& extension_id,
    const std::string& method_name,
    const base::ListValue& arguments,
    content::RenderView* render_view,
    const GURL& event_url) const {
  v8::HandleScope handle_scope;

  // We copy the context list, because calling into javascript may modify it
  // out from under us.
  ContextSet contexts = GetAll();

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  for (ContextSet::iterator it = contexts.begin(); it != contexts.end();
       ++it) {
    if ((*it)->v8_context().IsEmpty())
      continue;

    if (!extension_id.empty()) {
      const Extension* extension = (*it)->extension();
      if (!extension || (extension_id != extension->id()))
        continue;
    }

    content::RenderView* context_render_view = (*it)->GetRenderView();
    if (!context_render_view)
      continue;

    if (render_view && render_view != context_render_view)
      continue;

    if (!HasSufficientPermissions(context_render_view, event_url))
      continue;

    v8::Local<v8::Context> context(*((*it)->v8_context()));
    std::vector<v8::Handle<v8::Value> > v8_arguments;
    for (size_t i = 0; i < arguments.GetSize(); ++i) {
      const base::Value* item = NULL;
      CHECK(arguments.Get(i, &item));
      v8_arguments.push_back(converter->ToV8Value(item, context));
    }

    v8::Handle<v8::Value> retval;
    (*it)->CallChromeHiddenMethod(
        method_name, v8_arguments.size(), &v8_arguments[0], &retval);
  }
}

void ChromeV8ContextSet::OnExtensionUnloaded(const std::string& extension_id) {
  ContextSet contexts = GetAll();

  // Clean up contexts belonging to the unloaded extension. This is done so
  // that content scripts (which remain injected into the page) don't continue
  // receiving events and sending messages.
  for (ContextSet::iterator it = contexts.begin(); it != contexts.end();
       ++it) {
    if ((*it)->extension() && (*it)->extension()->id() == extension_id) {
      (*it)->DispatchOnUnloadEvent();
      Remove(*it);
    }
  }
}

}  // namespace extensions
