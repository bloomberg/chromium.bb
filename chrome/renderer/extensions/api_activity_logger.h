// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_API_ACTIVITY_LOGGER_H_
#define CHROME_RENDERER_EXTENSIONS_API_ACTIVITY_LOGGER_H_

#include <string>
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "v8/include/v8.h"

namespace extensions {

// Used to log extension API calls that are implemented with custom bindings.
// The events are sent via IPC to extensions::ActivityLog for recording and
// display.
class APIActivityLogger : public ChromeV8Extension {
 public:
  APIActivityLogger(Dispatcher* dispatcher, v8::Handle<v8::Context> v8_context);

  // This is ultimately invoked in schema_generated_bindings.js with
  // JavaScript arguments...
  //    arg0 - extension ID as a string
  //    arg1 - API call name as a string
  //    arg2 - arguments to the API call
  //    arg3 - any extra logging info as a string (optional)
  static v8::Handle<v8::Value> LogActivity(const v8::Arguments & args);

 private:
  DISALLOW_COPY_AND_ASSIGN(APIActivityLogger);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_API_ACTIVITY_LOGGER_H_

