// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_PEPPER_REQUEST_PROXY_H_
#define CHROME_RENDERER_EXTENSIONS_PEPPER_REQUEST_PROXY_H_

#include <map>
#include <string>

#include "base/callback.h"

namespace base {
class ListValue;
}

namespace v8 {
class Isolate;
}

namespace extensions {

class ChromeV8Context;

// A proxy that forwards pepper apps API calls through the Javascript API
// bindings.
class PepperRequestProxy {
 public:
  // A callback to be called with the result of an API call. If |success| is
  // true, |response| will contain the response value. Otherwise, |error| will
  // contain the error message.
  typedef base::Callback<void(bool success,
                              const base::ListValue& response,
                              const std::string& error)> ResponseCallback;

  explicit PepperRequestProxy(ChromeV8Context* context);
  ~PepperRequestProxy();

  // Starts an API request. Returns whether the call was successful. On failure,
  // |error| will contain the error message. |callback| will only be called on
  // success.
  bool StartRequest(const ResponseCallback& callback,
                    const std::string& request_name,
                    const base::ListValue& args,
                    std::string* error);

  void OnResponseReceived(int request_id,
                          bool success,
                          const base::ListValue& args,
                          const std::string& error);

 private:
  typedef std::map<int, ResponseCallback> PendingRequestMap;

  // Non-owning pointer.
  ChromeV8Context* context_;
  // Non-owning pointer.
  v8::Isolate* isolate_;
  PendingRequestMap pending_request_map_;
  int next_request_id_;
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_PEPPER_REQUEST_PROXY_H_
