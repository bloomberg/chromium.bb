// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_bindings_context.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/extensions/extension_base.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "content/common/url_constants.h"
#include "content/renderer/render_view.h"
#include "content/renderer/v8_value_converter.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"

namespace {

base::LazyInstance<ExtensionBindingsContext::ContextList> g_contexts(
    base::LINKER_INITIALIZED);

base::LazyInstance<std::string> g_testing_id(base::LINKER_INITIALIZED);

// Returns true if the extension running in the given |render_view| has
// sufficient permissions to access the data.
static bool HasSufficientPermissions(RenderView* render_view,
                                     const GURL& event_url) {
  // During unit tests, we might be invoked without a v8 context. In these
  // cases, we only allow empty event_urls and short-circuit before retrieving
  // the render view from the current context.
  if (!event_url.is_valid())
    return true;

  // TODO(aa): This looks super suspicious.
  WebKit::WebDocument document =
      render_view->webview()->mainFrame()->document();
  return GURL(document.url()).SchemeIs(chrome::kExtensionScheme) &&
       document.securityOrigin().canRequest(event_url);
}

}

void ExtensionBindingsContext::HandleV8ContextCreated(
    WebKit::WebFrame* web_frame,
    v8::Handle<v8::Context> v8_context,
    ExtensionDispatcher* extension_dispatcher,
    int isolated_world_id) {

  std::string extension_id;
  if (!g_testing_id.Get().empty()) {
    extension_id = g_testing_id.Get();
  } else if (isolated_world_id == 0) {
    // Figure out the frame's URL.  If the frame is loading, use its provisional
    // URL, since we get this notification before commit.
    WebKit::WebDataSource* ds = web_frame->provisionalDataSource();
    if (!ds)
      ds = web_frame->dataSource();
    GURL url = ds->request().url();
    const ExtensionSet* extensions = extension_dispatcher->extensions();
    extension_id = extensions->GetIdByURL(url);

    if (!extensions->ExtensionBindingsAllowed(url))
      return;
  } else {
    extension_id =
        extension_dispatcher->user_script_slave()->
            GetExtensionIdForIsolatedWorld(isolated_world_id);
  }

  ExtensionBindingsContext* instance =
      new ExtensionBindingsContext(v8_context, web_frame, extension_id);
  g_contexts.Pointer()->push_back(
      linked_ptr<ExtensionBindingsContext>(instance));
  VLOG(1) << "Num tracked contexts: " << g_contexts.Pointer()->size();

  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> argv[3];
  argv[0] = v8::String::New(extension_id.c_str());
  argv[1] = v8::Boolean::New(extension_dispatcher->is_extension_process());
  argv[2] = v8::Boolean::New(
      ChromeRenderProcessObserver::is_incognito_process());
  instance->CallChromeHiddenMethod("dispatchOnLoad", arraysize(argv), argv);
}

void ExtensionBindingsContext::HandleV8ContextReleased(
    WebKit::WebFrame* web_frame,
    v8::Handle<v8::Context> context,
    int isolated_world_id) {
  for (ContextList::iterator iter = g_contexts.Pointer()->begin();
       iter != g_contexts.Pointer()->end(); ++iter) {
    if ((*iter)->v8_context() == context) {
      iter = g_contexts.Pointer()->erase(iter);
      break;
    }
  }
  VLOG(1) << "Num tracked contexts: " << g_contexts.Pointer()->size();
}

ExtensionBindingsContext* ExtensionBindingsContext::GetCurrent() {
  if (!v8::Context::InContext())
    return NULL;
  else
    return GetByV8Context(v8::Context::GetCurrent());
}

ExtensionBindingsContext* ExtensionBindingsContext::GetByV8Context(
    const v8::Handle<v8::Context>& v8_context) {
  CHECK(!v8_context.IsEmpty());

  for (ContextList::iterator iter = g_contexts.Pointer()->begin();
       iter != g_contexts.Pointer()->end(); ++iter) {
    if ((*iter)->v8_context() == v8_context)
      return iter->get();
  }

  return NULL;
}

ExtensionBindingsContext::ContextList ExtensionBindingsContext::GetAll() {
  // Return a copy so we don't confuse ourselves if calling into them changes
  // the list underneath us.
  return g_contexts.Get();
}

void ExtensionBindingsContext::DispatchChromeHiddenMethod(
    const std::string& extension_id,
    const std::string& method_name,
    const base::ListValue& arguments,
    RenderView* render_view,
    const GURL& event_url) {
  v8::HandleScope handle_scope;

  // We copy the context list, because calling into javascript may modify it
  // out from under us. We also guard against deleted contexts by checking if
  // they have been cleared first.
  ContextList contexts = g_contexts.Get();

  V8ValueConverter converter;
  for (ContextList::iterator it = contexts.begin(); it != contexts.end();
       ++it) {
    if ((*it)->v8_context().IsEmpty())
      continue;

    if (!extension_id.empty() && extension_id != (*it)->extension_id())
      continue;

    if (!(*it)->web_frame()->view())
      continue;

    RenderView* context_render_view = (*it)->GetRenderView();
    if (!context_render_view)
      continue;

    if (render_view && render_view != context_render_view)
      continue;

    if (!HasSufficientPermissions(context_render_view, event_url))
      continue;

    v8::Local<v8::Context> context(*((*it)->v8_context()));
    std::vector<v8::Handle<v8::Value> > v8_arguments;
    for (size_t i = 0; i < arguments.GetSize(); ++i) {
      base::Value* item = NULL;
      CHECK(arguments.Get(i, &item));
      v8_arguments.push_back(converter.ToV8Value(item, context));
    }

    v8::Handle<v8::Value> retval = (*it)->CallChromeHiddenMethod(
        method_name, v8_arguments.size(), &v8_arguments[0]);
    // In debug, the js will validate the event parameters and return a
    // string if a validation error has occured.
    // TODO(rafaelw): Consider only doing this check if function_name ==
    // "Event.dispatchJSON".
#ifndef NDEBUG
    if (!retval.IsEmpty() && !retval->IsUndefined()) {
      std::string error = *v8::String::AsciiValue(retval);
      DCHECK(false) << error;
    }
#endif
  }
}

void ExtensionBindingsContext::SetTestExtensionId(const std::string& id) {
  g_testing_id.Get() = id;
}

ExtensionBindingsContext::ExtensionBindingsContext(
    v8::Handle<v8::Context> v8_context,
    WebKit::WebFrame* web_frame,
    const std::string& extension_id)
    : v8_context_(v8::Persistent<v8::Context>::New(v8_context)),
      web_frame_(web_frame),
      extension_id_(extension_id) {
  VLOG(1) << "Created context for extension\n"
          << "  id:    " << extension_id << "\n"
          << "  frame: " << web_frame_;
}

ExtensionBindingsContext::~ExtensionBindingsContext() {
  VLOG(1) << "Destroyed context for extension\n"
          << "  id:    " << extension_id_ << "\n"
          << "  frame: " << web_frame_;
  v8::HandleScope handle_scope;
  CallChromeHiddenMethod("dispatchOnUnload", 0, NULL);
  v8_context_.Dispose();
}

RenderView* ExtensionBindingsContext::GetRenderView() const {
  if (web_frame_->view())
    return RenderView::FromWebView(web_frame_->view());
  else
    return NULL;
}

v8::Handle<v8::Value> ExtensionBindingsContext::CallChromeHiddenMethod(
    const std::string& function_name,
    int argc,
    v8::Handle<v8::Value>* argv) {
  v8::Context::Scope context_scope(v8_context_);

  // Look up the function name, which may be a sub-property like
  // "Port.dispatchOnMessage" in the hidden global variable.
  v8::Local<v8::Value> value = v8::Local<v8::Value>::New(
      ExtensionBase::GetChromeHidden(v8_context_));
  std::vector<std::string> components;
  base::SplitStringDontTrim(function_name, '.', &components);
  for (size_t i = 0; i < components.size(); ++i) {
    if (!value.IsEmpty()) {
      value = v8::Local<v8::Object>::Cast(value)->Get(
          v8::String::New(components[i].c_str()));
    }
  }
  CHECK(!value.IsEmpty() && value->IsFunction());
  return v8::Local<v8::Function>::Cast(value)->Call(
      v8::Object::New(), argc, argv);
}
