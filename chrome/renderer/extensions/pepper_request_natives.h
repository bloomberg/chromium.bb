// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_PEPPER_REQUEST_NATIVES_H_
#define CHROME_RENDERER_EXTENSIONS_PEPPER_REQUEST_NATIVES_H_

#include "base/compiler_specific.h"
#include "extensions/renderer/object_backed_native_handler.h"

namespace base {
class Value;
}

namespace extensions {
class ChromeV8Context;

// Custom bindings for handling API calls from pepper plugins.
class PepperRequestNatives : public ObjectBackedNativeHandler {
 public:
  explicit PepperRequestNatives(ChromeV8Context* context);

 private:
  // Sends a response to an API call to the pepper plugin which made the call.
  // |args| should contain:
  //   |request_id|: An int containing the id of the request.
  //   |response|: An array containing the response.
  //   |error|: A string containing the error message if an error occurred or
  //            null if the call was successful.
  void SendResponse(const v8::FunctionCallbackInfo<v8::Value>& args);

  DISALLOW_COPY_AND_ASSIGN(PepperRequestNatives);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_PEPPER_REQUEST_NATIVES_H_
