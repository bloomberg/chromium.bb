// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_request_handler.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/renderer/bindings/exception_handler.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
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
          blink::WebUserGestureIndicator::CurrentUserGestureToken()) {
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
                                     APILastError last_error,
                                     ExceptionHandler* exception_handler)
    : send_request_(send_request),
      call_js_(call_js),
      last_error_(std::move(last_error)),
      exception_handler_(exception_handler) {}

APIRequestHandler::~APIRequestHandler() {}

int APIRequestHandler::StartRequest(v8::Local<v8::Context> context,
                                    const std::string& method,
                                    std::unique_ptr<base::ListValue> arguments,
                                    v8::Local<v8::Function> callback,
                                    v8::Local<v8::Function> custom_callback,
                                    binding::RequestThread thread) {
  auto request = std::make_unique<Request>();

  // The request id is primarily used in the renderer to associate an API
  // request with the associated callback, but it's also used in the browser as
  // an identifier for the extension function (e.g. by the pageCapture API).
  // TODO(devlin): We should probably fix this, since the request id is only
  // unique per-isolate, rather than globally.
  // TODO(devlin): We could *probably* get away with just using an integer
  // here, but it's a little less foolproof. How slow is GenerateGUID? Should
  // we use that instead? It means updating the IPC
  // (ExtensionHostMsg_Request).
  // base::UnguessableToken is another good option.
  int request_id = next_request_id_++;
  request->request_id = request_id;

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
      // bindings to get away from some of those. For now, just pass in an
      // object with the request id.
      v8::Local<v8::Object> request =
          gin::DataObjectBuilder(isolate).Set("id", request_id).Build();
      v8::Local<v8::Value> callback_to_pass = callback;
      if (callback_to_pass.IsEmpty())
        callback_to_pass = v8::Undefined(isolate);
      callback_args = {gin::StringToSymbol(isolate, method), request,
                       callback_to_pass};
      callback = custom_callback;
    }

    request->has_callback = true;
    pending_requests_.insert(std::make_pair(
        request_id, PendingRequest(isolate, callback, context, callback_args)));
  }

  request->has_user_gesture =
      blink::WebUserGestureIndicator::IsProcessingUserGestureThreadSafe();
  request->arguments = std::move(arguments);
  request->method_name = method;
  request->thread = thread;

  last_sent_request_id_ = request_id;
  send_request_.Run(std::move(request), context);
  return request_id;
}

void APIRequestHandler::CompleteRequest(int request_id,
                                        const base::ListValue& response_args,
                                        const std::string& error) {
  auto iter = pending_requests_.find(request_id);
  // The request may have been removed if the context was invalidated before a
  // response is ready.
  if (iter == pending_requests_.end())
    return;

  PendingRequest& pending_request = iter->second;

  v8::Isolate* isolate = pending_request.isolate;
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = pending_request.context.Get(isolate);
  v8::Context::Scope context_scope(context);
  std::unique_ptr<content::V8ValueConverter> converter =
      content::V8ValueConverter::Create();
  std::vector<v8::Local<v8::Value>> v8_args;
  v8_args.reserve(response_args.GetSize());
  for (const auto& arg : response_args)
    v8_args.push_back(converter->ToV8Value(&arg, context));

  // NOTE(devlin): This results in a double lookup of the pending request and an
  // extra Handle/Context-Scope, but that should be pretty cheap.
  CompleteRequest(request_id, v8_args, error);
}

void APIRequestHandler::CompleteRequest(
    int request_id,
    const std::vector<v8::Local<v8::Value>>& response_args,
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
  std::vector<v8::Local<v8::Value>> args;
  args.reserve(response_args.size() +
               pending_request.callback_arguments.size());
  for (const auto& arg : pending_request.callback_arguments)
    args.push_back(arg.Get(isolate));
  for (const auto& arg : response_args)
    args.push_back(arg);

  blink::WebScopedUserGesture user_gesture(pending_request.user_gesture_token);
  if (!error.empty())
    last_error_.SetError(context, error);

  v8::TryCatch try_catch(isolate);
  // args.size() is converted to int, but args is controlled by chrome and is
  // never close to std::numeric_limits<int>::max.
  call_js_.Run(pending_request.callback.Get(isolate), context, args.size(),
               args.data());
  if (try_catch.HasCaught()) {
    v8::Local<v8::Message> v8_message = try_catch.Message();
    base::Optional<std::string> message;
    if (!v8_message.IsEmpty())
      message = gin::V8ToString(v8_message->Get());
    exception_handler_->HandleException(context, "Error handling response",
                                        &try_catch);
  }

  if (!error.empty())
    last_error_.ClearError(context, true);
}

int APIRequestHandler::AddPendingRequest(v8::Local<v8::Context> context,
                                         v8::Local<v8::Function> callback) {
  int request_id = next_request_id_++;
  pending_requests_.emplace(
      request_id, PendingRequest(context->GetIsolate(), callback, context,
                                 std::vector<v8::Local<v8::Value>>()));
  return request_id;
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
