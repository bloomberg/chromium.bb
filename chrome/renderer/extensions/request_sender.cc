// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/request_sender.h"

#include "base/values.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"

using content::V8ValueConverter;

namespace extensions {

// Contains info relevant to a pending API request.
struct PendingRequest {
 public :
  PendingRequest(ChromeV8Context* context,
                 ChromeV8Context* caller_context,
                 const std::string& name)
      : name(name), context(context), caller_context(caller_context) {
  }

  std::string name;
  ChromeV8Context* context;
  ChromeV8Context* caller_context;
};

RequestSender::RequestSender(Dispatcher* dispatcher) : dispatcher_(dispatcher) {
}

RequestSender::~RequestSender() {
}

void RequestSender::InsertRequest(int request_id,
                                  PendingRequest* pending_request) {
  DCHECK_EQ(0u, pending_requests_.count(request_id));
  pending_requests_[request_id].reset(pending_request);
}

linked_ptr<PendingRequest> RequestSender::RemoveRequest(int request_id) {
  PendingRequestMap::iterator i = pending_requests_.find(request_id);
  if (i == pending_requests_.end())
    return linked_ptr<PendingRequest>();
  linked_ptr<PendingRequest> result = i->second;
  pending_requests_.erase(i);
  return result;
}

void RequestSender::StartRequest(ChromeV8Context* context,
                                 const std::string& name,
                                 int request_id,
                                 bool has_callback,
                                 bool for_io_thread,
                                 base::ListValue* value_args) {
  // Get the current RenderView so that we can send a routed IPC message from
  // the correct source.
  content::RenderView* renderview = context->GetRenderView();
  if (!renderview)
    return;

  const std::set<std::string>& function_names = dispatcher_->function_names();
  if (function_names.find(name) == function_names.end()) {
    NOTREACHED() << "Unexpected function " << name <<
        ". Did you remember to register it with ExtensionFunctionRegistry?";
    return;
  }

  // TODO(koz): See if we can make this a CHECK.
  if (!dispatcher_->CheckContextAccessToExtensionAPI(name, context))
    return;

  GURL source_url;
  WebKit::WebSecurityOrigin source_origin;
  WebKit::WebFrame* webframe = context->web_frame();
  if (webframe) {
    source_url = webframe->document().url();
    source_origin = webframe->document().securityOrigin();
  }

  std::string extension_id = context->GetExtensionID();
  // Insert the current context into the PendingRequest because that's the
  // context that we call back on.
  InsertRequest(
      request_id,
      new PendingRequest(context,
                         dispatcher_->v8_context_set().GetCurrent(),
                         name));

  ExtensionHostMsg_Request_Params params;
  params.name = name;
  params.arguments.Swap(value_args);
  params.extension_id = extension_id;
  params.source_url = source_url;
  params.source_origin = source_origin.toString();
  params.request_id = request_id;
  params.has_callback = has_callback;
  params.user_gesture =
      webframe ? webframe->isProcessingUserGesture() : false;
  if (for_io_thread) {
    renderview->Send(new ExtensionHostMsg_RequestForIOThread(
        renderview->GetRoutingID(), params));
  } else {
    renderview->Send(new ExtensionHostMsg_Request(
        renderview->GetRoutingID(), params));
  }
}

void RequestSender::HandleResponse(int request_id,
                                   bool success,
                                   const base::ListValue& responseList,
                                   const std::string& error) {
  linked_ptr<PendingRequest> request = RemoveRequest(request_id);

  if (!request.get()) {
    // This can happen if a context is destroyed while a request is in flight.
    return;
  }

  v8::HandleScope handle_scope;

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  v8::Handle<v8::Value> argv[] = {
    v8::Integer::New(request_id),
    v8::String::New(request->name.c_str()),
    v8::Boolean::New(success),
    converter->ToV8Value(&responseList, request->context->v8_context()),
    v8::String::New(error.c_str())
  };

  v8::Handle<v8::Value> retval;
  CHECK(request->context->CallChromeHiddenMethod("handleResponse",
                                                  arraysize(argv),
                                                  argv,
                                                  &retval));
  // In debug, the js will validate the callback parameters and return a
  // string if a validation error has occured.
  if (DCHECK_IS_ON()) {
    if (!retval.IsEmpty() && !retval->IsUndefined()) {
      std::string error = *v8::String::AsciiValue(retval);
      DCHECK(false) << error;
    }
  }
}

void RequestSender::InvalidateContext(ChromeV8Context* context) {
  for (PendingRequestMap::iterator it = pending_requests_.begin();
       it != pending_requests_.end();) {
    if (it->second->context == context)
      pending_requests_.erase(it++);
    else
      ++it;
  }
}

}  // namespace extensions
