// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_SCHEMA_GENERATED_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_SCHEMA_GENERATED_BINDINGS_H_
#pragma once

#include <string>

#include "chrome/renderer/extensions/chrome_v8_extension.h"

class ExtensionDispatcher;
class ChromeV8ContextSet;
class ChromeV8Extension;

namespace base {
class ListValue;
class Value;
}

namespace v8 {
class Extension;
}

namespace extensions {

// Generates JavaScript bindings for the extension system from the JSON
// declarations in chrome/common/extensions/api/.
// TODO(koz): Split this up so that GetNextRequestId/StartRequest and
// SetIconCommon are in separate classes.
class SchemaGeneratedBindings : public ChromeV8Extension {
 public:
  explicit SchemaGeneratedBindings(ExtensionDispatcher* extension_dispatcher);

  // Handles a response to an API request.  Sets |extension_id|.
  static void HandleResponse(const ChromeV8ContextSet& contexts,
                             int request_id,
                             bool success,
                             const std::string& response,
                             const std::string& error,
                             std::string* extension_id);

 private:
  v8::Handle<v8::Value> GetExtensionAPIDefinition(const v8::Arguments& args);
  v8::Handle<v8::Value> GetNextRequestId(const v8::Arguments& args);

  // Common code for starting an API request to the browser. |value_args|
  // contains the request's arguments.
  // Steals value_args contents for efficiency.
  v8::Handle<v8::Value> StartRequestCommon(const v8::Arguments& args,
                                           base::ListValue* value_args);

  // Starts an API request to the browser, with an optional callback.  The
  // callback will be dispatched to EventBindings::HandleResponse.
  v8::Handle<v8::Value> StartRequest(const v8::Arguments& args);

  bool ConvertImageDataToBitmapValue(const v8::Arguments& args,
                                     base::Value** bitmap_value);

  // A special request for setting the extension action icon. This function
  // accepts a canvas ImageData object, so it needs to do extra processing
  // before sending the request to the browser.
  v8::Handle<v8::Value> SetIconCommon(const v8::Arguments& args);

  DISALLOW_COPY_AND_ASSIGN(SchemaGeneratedBindings);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_SCHEMA_GENERATED_BINDINGS_H_
