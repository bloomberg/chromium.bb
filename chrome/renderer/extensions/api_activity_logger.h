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

// Used to log extension API calls and events that are implemented with custom
// bindings.The actions are sent via IPC to extensions::ActivityLog for
// recording and display.
class APIActivityLogger : public ChromeV8Extension {
 public:
  APIActivityLogger(Dispatcher* dispatcher, v8::Handle<v8::Context> v8_context);

  // This is ultimately invoked in schema_generated_bindings.js with
  // JavaScript arguments. Logged as an APIAction.
  //    arg0 - extension ID as a string
  //    arg1 - API call name as a string
  //    arg2 - arguments to the API call
  //    arg3 - any extra logging info as a string (optional)
  static v8::Handle<v8::Value> LogAPICall(const v8::Arguments& args);

  // This is ultimately invoked in schema_generated_bindings.js with
  // JavaScript arguments. Logged as an EventAction.
  //    arg0 - extension ID as a string
  //    arg1 - Event name as a string
  //    arg2 - Event arguments
  //    arg3 - any extra logging info as a string (optional)
  static v8::Handle<v8::Value> LogEvent(const v8::Arguments& args);

 private:
   enum CallType {
     APICALL,
     EVENT
   };

   // LogAPICall and LogEvent are really the same underneath except for
   // how they are ultimately dispatched to the log.
   static void LogInternal(const CallType call_type,
                           const v8::Arguments& args);

  DISALLOW_COPY_AND_ASSIGN(APIActivityLogger);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_API_ACTIVITY_LOGGER_H_

