// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_REQUEST_HANDLER_H_
#define EXTENSIONS_RENDERER_API_REQUEST_HANDLER_H_

#include <map>
#include <memory>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "extensions/renderer/api_last_error.h"
#include "third_party/WebKit/public/web/WebUserGestureToken.h"
#include "v8/include/v8.h"

namespace base {
class ListValue;
}

namespace extensions {

// A wrapper around a map for extension API calls. Contains all pending requests
// and the associated context and callback. Designed to be used on a single
// thread, but amongst multiple contexts.
class APIRequestHandler {
 public:
  // TODO(devlin): We may want to coalesce this with the
  // ExtensionHostMsg_Request_Params IPC struct.
  struct Request {
    Request();
    ~Request();

    int request_id = -1;
    std::string method_name;
    bool has_callback = false;
    bool has_user_gesture = false;
    std::unique_ptr<base::ListValue> arguments;

   private:
    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  using SendRequestMethod =
      base::Callback<void(std::unique_ptr<Request>, v8::Local<v8::Context>)>;

  using CallJSFunction = base::Callback<void(v8::Local<v8::Function>,
                                             v8::Local<v8::Context>,
                                             int argc,
                                             v8::Local<v8::Value>[])>;

  APIRequestHandler(const SendRequestMethod& send_request,
                    const CallJSFunction& call_js,
                    APILastError last_error);
  ~APIRequestHandler();

  // Begins the process of processing the request. Returns the identifier of the
  // pending request, or -1 if no pending request was added (which can happen if
  // no callback was specified).
  int StartRequest(v8::Local<v8::Context> context,
                   const std::string& method,
                   std::unique_ptr<base::ListValue> arguments,
                   v8::Local<v8::Function> callback,
                   v8::Local<v8::Function> custom_callback);

  // Responds to the request with the given |request_id|, calling the callback
  // with the given |response| arguments.
  // Invalid ids are ignored.
  void CompleteRequest(int request_id,
                       const base::ListValue& response,
                       const std::string& error);

  // Invalidates any requests that are associated with |context|.
  void InvalidateContext(v8::Local<v8::Context> context);

  std::set<int> GetPendingRequestIdsForTesting() const;

 private:
  struct PendingRequest {
    PendingRequest(v8::Isolate* isolate,
                   v8::Local<v8::Function> callback,
                   v8::Local<v8::Context> context,
                   const std::vector<v8::Local<v8::Value>>& callback_args);
    ~PendingRequest();
    PendingRequest(PendingRequest&&);
    PendingRequest& operator=(PendingRequest&&);

    v8::Isolate* isolate;
    v8::Global<v8::Context> context;
    v8::Global<v8::Function> callback;
    std::vector<v8::Global<v8::Value>> callback_arguments;
    blink::WebUserGestureToken user_gesture_token;
  };

  // The next available request identifier.
  int next_request_id_ = 0;

  // A map of all pending requests.
  std::map<int, PendingRequest> pending_requests_;

  SendRequestMethod send_request_;

  // The method to call into a JS with specific arguments. We curry this in
  // because the manner we want to do this is a unittest (e.g.
  // v8::Function::Call) can be significantly different than in production
  // (where we have to deal with e.g. blocking javascript).
  CallJSFunction call_js_;

  APILastError last_error_;

  DISALLOW_COPY_AND_ASSIGN(APIRequestHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_REQUEST_HANDLER_H_
