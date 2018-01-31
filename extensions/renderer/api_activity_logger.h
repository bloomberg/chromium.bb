// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_ACTIVITY_LOGGER_H_
#define EXTENSIONS_RENDERER_API_ACTIVITY_LOGGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8.h"

namespace base {
class ListValue;
}

namespace extensions {

// Used to log extension API calls and events that are implemented with custom
// bindings.The actions are sent via IPC to extensions::ActivityLog for
// recording and display.
class APIActivityLogger : public ObjectBackedNativeHandler {
 public:
  explicit APIActivityLogger(ScriptContext* context);
  ~APIActivityLogger() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

  // Returns true if logging is enabled.
  static bool IsLoggingEnabled();

  // Notifies the browser that an API method has been called, if and only if
  // activity logging is enabled.
  static void LogAPICall(v8::Local<v8::Context> context,
                         const std::string& call_name,
                         const std::vector<v8::Local<v8::Value>>& arguments);

  // Notifies the browser that an API event has been dispatched, if and only if
  // activity logging is enabled.
  static void LogEvent(ScriptContext* script_context,
                       const std::string& event_name,
                       std::unique_ptr<base::ListValue> arguments);

  static void set_log_for_testing(bool log);

 private:
  // Used to distinguish API calls & events from each other in LogInternal.
  enum CallType { APICALL, EVENT };

  // This is ultimately invoked in bindings.js with JavaScript arguments.
  //    arg0 - extension ID as a string
  //    arg1 - API method/Event name as a string
  //    arg2 - arguments to the API call/event
  //    arg3 - any extra logging info as a string (optional)
  // TODO(devlin): Does arg3 ever exist?
  void LogForJS(const CallType call_type,
                const v8::FunctionCallbackInfo<v8::Value>& args);

  // Common implementation method for sending a logging IPC.
  static void LogInternal(const CallType call_type,
                          const std::string& extension_id,
                          const std::string& call_name,
                          std::unique_ptr<base::ListValue> arguments,
                          const std::string& extra);

  DISALLOW_COPY_AND_ASSIGN(APIActivityLogger);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_ACTIVITY_LOGGER_H_
