// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/web_request_custom_bindings.h"

#include <string>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

namespace extensions {

WebRequestCustomBindings::WebRequestCustomBindings(
    Dispatcher* dispatcher, ChromeV8Context* context)
    : ChromeV8Extension(dispatcher, context) {
  RouteFunction("GetUniqueSubEventName",
      base::Bind(&WebRequestCustomBindings::GetUniqueSubEventName,
                 base::Unretained(this)));
}

// Attach an event name to an object.
void WebRequestCustomBindings::GetUniqueSubEventName(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  static int next_event_id = 0;
  DCHECK(args.Length() == 1);
  DCHECK(args[0]->IsString());
  std::string event_name(*v8::String::AsciiValue(args[0]));
  std::string unique_event_name =
      event_name + "/" + base::IntToString(++next_event_id);
  args.GetReturnValue().Set(v8::String::New(unique_event_name.c_str()));
}

}  // namespace extensions
