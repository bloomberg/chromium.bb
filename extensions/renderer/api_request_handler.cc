// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_request_handler.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "gin/converter.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"

namespace extensions {

APIRequestHandler::Request::Request() {}
APIRequestHandler::Request::~Request() = default;

APIRequestHandler::PendingRequest::PendingRequest(
    v8::Isolate* isolate,
    v8::Local<v8::Function> callback,
    v8::Local<v8::Context> context,
    const std::vector<v8::Local<v8::Value>>& local_callback_args)
    : isolate(isolate),
      context(isolate, context),
      callback(isolate, callback),
      user_gesture_token(
          blink::WebUserGestureIndicator::currentUserGestureToken()) {
  if (!local_callback_args.empty()) {
    callback_arguments.reserve(local_callback_args.size());
    for (const auto& arg : local_callback_args)
      callback_arguments.push_back(v8::Global<v8::Value>(isolate, arg));
  }
}

APIRequestHandler::PendingRequest::~PendingRequest() {}
APIRequestHandler::PendingRequest::PendingRequest(PendingRequest&&) = default;
APIRequestHandler::PendingRequest& APIRequestHandler::PendingRequest::operator=(
    PendingRequest&&) = default;

APIRequestHandler::APIRequestHandler(const SendRequestMethod& send_request,
                                     const CallJSFunction& call_js,
                                     APILastError last_error)
    : send_request_(send_request),
      call_js_(call_js),
      last_error_(std::move(last_error)) {}

APIRequestHandler::~APIRequestHandler() {}

int APIRequestHandler::StartRequest(v8::Local<v8::Context> context,
                                    const std::string& method,
                                    std::unique_ptr<base::ListValue> arguments,
                                    v8::Local<v8::Function> callback,
                                    v8::Local<v8::Function> custom_callback) {
  auto request = base::MakeUnique<Request>();

  if (!custom_callback.IsEmpty() || !callback.IsEmpty()) {
    v8::Isolate* isolate = context->GetIsolate();
    // In the JS bindings, custom callbacks are called with the arguments of
    // name, the full request object (see below), the original callback, and
    // the responses from the API. The responses from the API are handled by the
    // APIRequestHandler, but we need to curry in the other values.
    std::vector<v8::Local<v8::Value>> callback_args;
    if (!custom_callback.IsEmpty()) {
      // TODO(devlin): The |request| object in the JS bindings includes
      // properties for callback, callbackSchema, args, stack, id, and
      // customCallback. Of those, it appears that we only use stack, args, and
      // id (since callback is curried in separately). We may be able to update
      // bindings to get away from some of those. For now, just pass in an empty
      // object (most APIs don't rely on it).
      v8::Local<v8::Object> request = v8::Object::New(isolate);
      callback_args = {gin::StringToSymbol(isolate, method), request, callback};
      callback = custom_callback;
    }

    // TODO(devlin): We could *probably* get away with just using an integer
    // here, but it's a little less foolproof. How slow is GenerateGUID? Should
    // we use that instead? It means updating the IPC
    // (ExtensionHostMsg_Request).
    // base::UnguessableToken is another good option.
    request->request_id = next_request_id_++;
    request->has_callback = true;
    pending_requests_.insert(std::make_pair(
        request->request_id,
        PendingRequest(isolate, callback, context, callback_args)));
  }

  request->has_user_gesture =
      blink::WebUserGestureIndicator::isProcessingUserGestureThreadSafe();
  request->arguments = std::move(arguments);
  request->method_name = method;

  int id = request->request_id;
  send_request_.Run(std::move(request), context);
  return id;
}

void APIRequestHandler::CompleteRequest(int request_id,
                                        const base::ListValue& response_args,
                                        const std::string& error) {
  auto iter = pending_requests_.find(request_id);
  // The request may have been removed if the context was invalidated before a
  // response is ready.
  if (iter == pending_requests_.end())
    return;

  PendingRequest pending_request = std::move(iter->second);
  pending_requests_.erase(iter);

  v8::Isolate* isolate = pending_request.isolate;
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = pending_request.context.Get(isolate);
  v8::Context::Scope context_scope(context);
  std::unique_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  std::vector<v8::Local<v8::Value>> args;
  args.reserve(response_args.GetSize() +
               pending_request.callback_arguments.size());
  for (const auto& arg : pending_request.callback_arguments)
    args.push_back(arg.Get(isolate));
  for (const auto& arg : response_args)
    args.push_back(converter->ToV8Value(arg.get(), context));

  blink::WebScopedUserGesture user_gesture(pending_request.user_gesture_token);
  if (!error.empty())
    last_error_.SetError(context, error);

  // args.size() is converted to int, but args is controlled by chrome and is
  // never close to std::numeric_limits<int>::max.
  call_js_.Run(pending_request.callback.Get(isolate), context, args.size(),
               args.data());

  if (!error.empty())
    last_error_.ClearError(context, true);
}

void APIRequestHandler::InvalidateContext(v8::Local<v8::Context> context) {
  for (auto iter = pending_requests_.begin();
       iter != pending_requests_.end();) {
    if (iter->second.context == context)
      iter = pending_requests_.erase(iter);
    else
      ++iter;
  }
}

std::set<int> APIRequestHandler::GetPendingRequestIdsForTesting() const {
  std::set<int> result;
  for (const auto& pair : pending_requests_)
    result.insert(pair.first);
  return result;
}

}  // namespace extensions
