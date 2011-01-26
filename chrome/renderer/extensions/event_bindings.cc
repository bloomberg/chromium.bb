// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/event_bindings.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/extension_renderer_info.h"
#include "chrome/renderer/extensions/js_only_v8_extensions.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "googleurl/src/gurl.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using bindings_utils::CallFunctionInContext;
using bindings_utils::ContextInfo;
using bindings_utils::ContextList;
using bindings_utils::GetContexts;
using bindings_utils::GetInfoForCurrentContext;
using bindings_utils::GetStringResource;
using bindings_utils::ExtensionBase;
using bindings_utils::GetPendingRequestMap;
using bindings_utils::PendingRequestMap;
using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebSecurityOrigin;
using WebKit::WebURL;

static void ContextWeakReferenceCallback(v8::Persistent<v8::Value> context,
                                         void*);

namespace {

// Keep a local cache of RenderThread so that we can mock it out for unit tests.
static RenderThreadBase* render_thread = NULL;
static bool in_unit_tests = false;

// Set to true if these bindings are registered.  Will be false when extensions
// are disabled.
static bool bindings_registered = false;


// A map of event names to the number of listeners for that event. We notify
// the browser about event listeners when we transition between 0 and 1.
typedef std::map<std::string, int> EventListenerCounts;

struct SingletonData {
  // A map of extension IDs to listener counts for that extension.
  std::map<std::string, EventListenerCounts> listener_counts_;
};

static base::LazyInstance<SingletonData> g_singleton_data(
    base::LINKER_INITIALIZED);

static EventListenerCounts& GetListenerCounts(const std::string& extension_id) {
  return g_singleton_data.Get().listener_counts_[extension_id];
}

class ExtensionImpl : public ExtensionBase {
 public:
  ExtensionImpl()
      : ExtensionBase(EventBindings::kName,
                      GetStringResource(IDR_EVENT_BINDINGS_JS),
                      0, NULL) {
  }
  ~ExtensionImpl() {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("AttachEvent"))) {
      return v8::FunctionTemplate::New(AttachEvent);
    } else if (name->Equals(v8::String::New("DetachEvent"))) {
      return v8::FunctionTemplate::New(DetachEvent);
    }
    return ExtensionBase::GetNativeFunction(name);
  }

  // Attach an event name to an object.
  static v8::Handle<v8::Value> AttachEvent(const v8::Arguments& args) {
    DCHECK(args.Length() == 1);
    // TODO(erikkay) should enforce that event name is a string in the bindings
    DCHECK(args[0]->IsString() || args[0]->IsUndefined());

    if (args[0]->IsString()) {
      ContextInfo* context_info = GetInfoForCurrentContext();
      EventListenerCounts& listener_counts =
          GetListenerCounts(context_info->extension_id);
      std::string event_name(*v8::String::AsciiValue(args[0]));
      bool has_permission =
          ExtensionProcessBindings::CurrentContextHasPermission(event_name);

      if (!has_permission) {
        return ExtensionProcessBindings::ThrowPermissionDeniedException(
            event_name);
      }

      if (++listener_counts[event_name] == 1) {
        EventBindings::GetRenderThread()->Send(
            new ViewHostMsg_ExtensionAddListener(context_info->extension_id,
                                                 event_name));
      }

      if (++context_info->num_connected_events == 1)
        context_info->context.ClearWeak();

    }

    return v8::Undefined();
  }

  static v8::Handle<v8::Value> DetachEvent(const v8::Arguments& args) {
    DCHECK(args.Length() == 1);
    // TODO(erikkay) should enforce that event name is a string in the bindings
    DCHECK(args[0]->IsString() || args[0]->IsUndefined());

    if (args[0]->IsString()) {
      ContextInfo* context_info = GetInfoForCurrentContext();
      if (!context_info)
        return v8::Undefined();

      EventListenerCounts& listener_counts =
          GetListenerCounts(context_info->extension_id);
      std::string event_name(*v8::String::AsciiValue(args[0]));
      if (--listener_counts[event_name] == 0) {
        EventBindings::GetRenderThread()->Send(
            new ViewHostMsg_ExtensionRemoveListener(context_info->extension_id,
                                                    event_name));
      }

      if (--context_info->num_connected_events == 0) {
        context_info->context.MakeWeak(NULL, &ContextWeakReferenceCallback);
      }
    }

    return v8::Undefined();
  }
};

// Returns true if the extension running in the given |context| has sufficient
// permissions to access the data.
static bool HasSufficientPermissions(ContextInfo* context,
                                     const GURL& event_url) {
  v8::Context::Scope context_scope(context->context);

  // During unit tests, we might be invoked without a v8 context. In these
  // cases, we only allow empty event_urls and short-circuit before retrieving
  // the render view from the current context.
  if (!event_url.is_valid())
    return true;

  RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
  bool url_permissions_ok = (!event_url.is_valid() ||
      (renderview &&
       GURL(renderview->webview()->mainFrame()->url()).SchemeIs(
           chrome::kExtensionScheme) &&
       renderview->webview()->mainFrame()->securityOrigin().canRequest(
          event_url)));
  return url_permissions_ok;
}

}  // namespace

const char* EventBindings::kName = "chrome/EventBindings";
const char* EventBindings::kTestingExtensionId =
    "oooooooooooooooooooooooooooooooo";

v8::Extension* EventBindings::Get() {
  static v8::Extension* extension = new ExtensionImpl();
  bindings_registered = true;
  return extension;
}

// static
void EventBindings::SetRenderThread(RenderThreadBase* thread) {
  render_thread = thread;
  in_unit_tests = true;
}

// static
RenderThreadBase* EventBindings::GetRenderThread() {
  return render_thread ? render_thread : RenderThread::current();
}

static void DeferredUnload(v8::Persistent<v8::Context> context) {
  v8::HandleScope handle_scope;
  CallFunctionInContext(context, "dispatchOnUnload", 0, NULL);
  context.Dispose();
  context.Clear();
}

static void UnregisterContext(ContextList::iterator context_iter, bool in_gc) {
  // Notify the bindings that they're going away.
  if (in_gc) {
    // We shouldn't call back into javascript during a garbage collect.  Do it
    // later.  We'll hang onto the context until this DeferredUnload is called.
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableFunction(
        DeferredUnload, (*context_iter)->context));
  } else {
    CallFunctionInContext((*context_iter)->context, "dispatchOnUnload",
                          0, NULL);
  }

  // Remove all pending requests for this context.
  PendingRequestMap& pending_requests = GetPendingRequestMap();
  for (PendingRequestMap::iterator it = pending_requests.begin();
       it != pending_requests.end(); ) {
    PendingRequestMap::iterator current = it++;
    if (current->second->context == (*context_iter)->context) {
      current->second->context.Dispose();
      current->second->context.Clear();
      pending_requests.erase(current);
    }
  }

  // Remove it from our registered contexts.
  (*context_iter)->context.ClearWeak();
  if (!in_gc) {
    (*context_iter)->context.Dispose();
    (*context_iter)->context.Clear();
  }

  GetContexts().erase(context_iter);
}

static void ContextWeakReferenceCallback(v8::Persistent<v8::Value> context,
                                         void*) {
  // This should only get called for content script contexts.
  for (ContextList::iterator it = GetContexts().begin();
       it != GetContexts().end(); ++it) {
    if ((*it)->context == context) {
      UnregisterContext(it, true);
      return;
    }
  }

  NOTREACHED();
}

void EventBindings::HandleContextCreated(WebFrame* frame, bool content_script) {
  if (!bindings_registered)
    return;

  v8::HandleScope handle_scope;
  ContextList& contexts = GetContexts();
  v8::Local<v8::Context> frame_context = frame->mainWorldScriptContext();
  v8::Local<v8::Context> context = v8::Context::GetCurrent();
  DCHECK(!context.IsEmpty());
  DCHECK(bindings_utils::FindContext(context) == contexts.end());

  // Figure out the frame's URL.  If the frame is loading, use its provisional
  // URL, since we get this notification before commit.
  WebDataSource* ds = frame->provisionalDataSource();
  if (!ds)
    ds = frame->dataSource();
  GURL url = ds->request().url();
  const ExtensionRendererInfo* extensions = GetRenderThread()->GetExtensions();
  std::string extension_id = extensions->GetIdByURL(url);

  if (!extensions->ExtensionBindingsAllowed(url) &&
      !content_script) {
    // This context is a regular non-extension web page or an unprivileged
    // chrome app. Ignore it. We only care about content scripts and extension
    // frames.
    // (Unless we're in unit tests, in which case we don't care what the URL
    // is).
    DCHECK(frame_context.IsEmpty() || frame_context == context);
    if (!in_unit_tests)
      return;

    // For tests, we want the dispatchOnLoad to actually setup our bindings,
    // so we give a fake extension id;
    extension_id = kTestingExtensionId;
  }

  v8::Persistent<v8::Context> persistent_context =
      v8::Persistent<v8::Context>::New(context);
  WebFrame* parent_frame = NULL;

  if (content_script) {
    DCHECK(frame_context != context);
    parent_frame = frame;
    // Content script contexts can get GCed before their frame goes away, so
    // set up a GC callback.
    persistent_context.MakeWeak(NULL, &ContextWeakReferenceCallback);
  }

  RenderView* render_view = NULL;
  if (frame->view())
    render_view = RenderView::FromWebView(frame->view());

  contexts.push_back(linked_ptr<ContextInfo>(
      new ContextInfo(persistent_context, extension_id, parent_frame,
                      render_view)));

  // Content scripts get initialized in user_script_slave.cc.
  if (!content_script) {
    v8::Handle<v8::Value> argv[1];
    argv[0] = v8::String::New(extension_id.c_str());
    CallFunctionInContext(context, "dispatchOnLoad", arraysize(argv), argv);
  }
}

// static
void EventBindings::HandleContextDestroyed(WebFrame* frame) {
  if (!bindings_registered)
    return;

  v8::HandleScope handle_scope;
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  if (!context.IsEmpty()) {
    ContextList::iterator context_iter = bindings_utils::FindContext(context);
    if (context_iter != GetContexts().end())
      UnregisterContext(context_iter, false);
  }

  // Unload any content script contexts for this frame.  Note that the frame
  // itself might not be registered, but can still be a parent frame.
  for (ContextList::iterator it = GetContexts().begin();
       it != GetContexts().end(); ) {
    if ((*it)->parent_frame == frame) {
      UnregisterContext(it, false);
      // UnregisterContext will remove |it| from the list, but may also
      // modify the rest of the list as a result of calling into javascript.
      it = GetContexts().begin();
    } else {
      ++it;
    }
  }
}

// static
void EventBindings::CallFunction(const std::string& extension_id,
                                 const std::string& function_name,
                                 int argc, v8::Handle<v8::Value>* argv,
                                 RenderView* render_view,
                                 const GURL& event_url) {
  // We copy the context list, because calling into javascript may modify it
  // out from under us. We also guard against deleted contexts by checking if
  // they have been cleared first.
  ContextList contexts = GetContexts();

  for (ContextList::iterator it = contexts.begin();
       it != contexts.end(); ++it) {
    if (render_view && render_view != (*it)->render_view)
      continue;

    if (!extension_id.empty() && extension_id != (*it)->extension_id)
      continue;

    if ((*it)->context.IsEmpty())
      continue;

    if (!HasSufficientPermissions(it->get(), event_url))
      continue;

    v8::Handle<v8::Value> retval = CallFunctionInContext((*it)->context,
        function_name, argc, argv);
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
