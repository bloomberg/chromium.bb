// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_request_sender.h"

#include "base/values.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_context_set.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"

using content::V8ValueConverter;

// Contains info relevant to a pending API request.
struct PendingRequest {
 public :
  PendingRequest(v8::Persistent<v8::Context> context, const std::string& name,
                 const std::string& extension_id)
      : context(context), name(name), extension_id(extension_id) {
  }

  ~PendingRequest() {
    context.Dispose();
  }

  v8::Persistent<v8::Context> context;
  std::string name;
  std::string extension_id;
};

ExtensionRequestSender::ExtensionRequestSender(
    ExtensionDispatcher* extension_dispatcher,
    ChromeV8ContextSet* context_set)
    : extension_dispatcher_(extension_dispatcher),
      context_set_(context_set) {
}

ExtensionRequestSender::~ExtensionRequestSender() {
}

void ExtensionRequestSender::InsertRequest(int request_id,
                                           PendingRequest* pending_request) {
  DCHECK_EQ(0u, pending_requests_.count(request_id));
  pending_requests_[request_id].reset(pending_request);
}

linked_ptr<PendingRequest> ExtensionRequestSender::RemoveRequest(
    int request_id) {
  PendingRequestMap::iterator i = pending_requests_.find(request_id);
  if (i == pending_requests_.end())
    return linked_ptr<PendingRequest>();
  linked_ptr<PendingRequest> result = i->second;
  pending_requests_.erase(i);
  return result;
}

void ExtensionRequestSender::StartRequest(
    const std::string& name,
    int request_id,
    bool has_callback,
    bool for_io_thread,
    base::ListValue* value_args) {
  ChromeV8Context* current_context = context_set_->GetCurrent();
  if (!current_context)
    return;

  // Get the current RenderView so that we can send a routed IPC message from
  // the correct source.
  content::RenderView* renderview = current_context->GetRenderView();
  if (!renderview)
    return;

  const std::set<std::string>& function_names =
      extension_dispatcher_->function_names();
  if (function_names.find(name) == function_names.end()) {
    NOTREACHED() << "Unexpected function " << name <<
        ". Did you remember to register it with ExtensionFunctionRegistry?";
    return;
  }

  // TODO(koz): See if we can make this a CHECK.
  if (!extension_dispatcher_->CheckCurrentContextAccessToExtensionAPI(name))
    return;

  GURL source_url;
  WebKit::WebSecurityOrigin source_origin;
  WebKit::WebFrame* webframe = current_context->web_frame();
  if (webframe) {
    source_url = webframe->document().url();
    source_origin = webframe->document().securityOrigin();
  }

  v8::Persistent<v8::Context> v8_context =
      v8::Persistent<v8::Context>::New(v8::Context::GetCurrent());
  DCHECK(!v8_context.IsEmpty());

  std::string extension_id = current_context->GetExtensionID();
  InsertRequest(request_id, new PendingRequest(
      v8_context, name, extension_id));

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

void ExtensionRequestSender::HandleResponse(int request_id,
                                            bool success,
                                            const base::ListValue& response,
                                            const std::string& error) {
  linked_ptr<PendingRequest> request = RemoveRequest(request_id);

  if (!request.get()) {
    // This should not be able to happen since we only remove requests when
    // they are handled.
    LOG(ERROR) << "Could not find specified request id: " << request_id;
    return;
  }

  ChromeV8Context* v8_context = context_set_->GetByV8Context(request->context);
  if (!v8_context)
    return;  // The frame went away.

  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> response_value = v8::Undefined();
  DCHECK(response.GetSize() <= 1);
  if (response.GetSize() == 1) {
    // We use a ListValue as a wrapper (our IPC system can't handle Value
    // directly since it has a private constructor) so we need to extract the
    // value at index 0.
    Value* value = NULL;
    CHECK(response.Get(0, &value));
    CHECK(value);
    scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
    response_value = converter->ToV8Value(value, v8_context->v8_context());
  }

  v8::Handle<v8::Value> argv[5];
  argv[0] = v8::Integer::New(request_id);
  argv[1] = v8::String::New(request->name.c_str());
  argv[2] = v8::Boolean::New(success);
  argv[3] = response_value;
  argv[4] = v8::String::New(error.c_str());

  v8::Handle<v8::Value> retval;
  CHECK(v8_context->CallChromeHiddenMethod("handleResponse",
                                           arraysize(argv),
                                           argv,
                                           &retval));
  // In debug, the js will validate the callback parameters and return a
  // string if a validation error has occured.
#ifndef NDEBUG
  if (!retval.IsEmpty() && !retval->IsUndefined()) {
    std::string error = *v8::String::AsciiValue(retval);
    DCHECK(false) << error;
  }
#endif
}
