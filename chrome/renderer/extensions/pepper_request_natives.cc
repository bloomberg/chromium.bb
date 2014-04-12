// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/pepper_request_natives.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "content/public/renderer/v8_value_converter.h"

namespace extensions {

PepperRequestNatives::PepperRequestNatives(ChromeV8Context* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "SendResponse",
      base::Bind(&PepperRequestNatives::SendResponse, base::Unretained(this)));
}

void PepperRequestNatives::SendResponse(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_EQ(3, args.Length());
  DCHECK(args[0]->IsInt32());
  DCHECK(args[1]->IsArray());
  int request_id = args[0]->Int32Value();

  // TODO(rockot): This downcast should be eliminated.
  // See http://crbug.com/362616.
  ChromeV8Context* chrome_context = static_cast<ChromeV8Context*>(context());
  if (args[2]->IsString()) {
    chrome_context->pepper_request_proxy()->OnResponseReceived(
        request_id, false, base::ListValue(), *v8::String::Utf8Value(args[2]));
    return;
  }

  scoped_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  scoped_ptr<const base::Value> result(
      converter->FromV8Value(args[1], chrome_context->v8_context()));
  DCHECK(result);
  const base::ListValue* result_list = NULL;
  CHECK(result->GetAsList(&result_list));
  chrome_context->pepper_request_proxy()->OnResponseReceived(
      request_id, true, *result_list, "");
}

}  // namespace extensions
