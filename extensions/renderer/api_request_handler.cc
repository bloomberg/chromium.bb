// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_request_handler.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"

namespace extensions {

APIRequestHandler::PendingRequest::PendingRequest(
    v8::Isolate* isolate,
    v8::Local<v8::Function> callback,
    v8::Local<v8::Context> context,
    const std::vector<v8::Local<v8::Value>>& local_callback_args)
    : isolate(isolate), context(isolate, context), callback(isolate, callback) {
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

APIRequestHandler::APIRequestHandler(const CallJSFunction& call_js)
    : call_js_(call_js) {}

APIRequestHandler::~APIRequestHandler() {}

int APIRequestHandler::AddPendingRequest(
    v8::Isolate* isolate,
    v8::Local<v8::Function> callback,
    v8::Local<v8::Context> context,
    const std::vector<v8::Local<v8::Value>>& callback_args) {
  // TODO(devlin): We could *probably* get away with just using an integer here,
  // but it's a little less foolproof. How slow is GenerateGUID? Should we use
  // that instead? It means updating the IPC (ExtensionHostMsg_Request).
  // base::UnguessableToken is another good option.
  int id = next_request_id_++;
  pending_requests_.insert(std::make_pair(
      id, PendingRequest(isolate, callback, context, callback_args)));
  return id;
}

void APIRequestHandler::CompleteRequest(int request_id,
                                        const base::ListValue& response_args) {
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
  std::unique_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  std::vector<v8::Local<v8::Value>> args;
  args.reserve(response_args.GetSize() +
               pending_request.callback_arguments.size());
  for (const auto& arg : pending_request.callback_arguments)
    args.push_back(arg.Get(isolate));
  for (const auto& arg : response_args)
    args.push_back(converter->ToV8Value(arg.get(), context));

  // args.size() is converted to int, but args is controlled by chrome and is
  // never close to std::numeric_limits<int>::max.
  call_js_.Run(pending_request.callback.Get(isolate), context, args.size(),
               args.data());
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
