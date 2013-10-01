// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/id_generator_custom_bindings.h"

#include "base/bind.h"
#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

namespace extensions {

IdGeneratorCustomBindings::IdGeneratorCustomBindings(Dispatcher* dispatcher,
                                                     ChromeV8Context* context)
    : ChromeV8Extension(dispatcher, context) {
  RouteFunction("GetNextId", base::Bind(&IdGeneratorCustomBindings::GetNextId,
                                        base::Unretained(this)));
}

void IdGeneratorCustomBindings::GetNextId(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  static int32_t next_id = 0;
  ++next_id;
  // Make sure 0 is never returned because some APIs (particularly WebRequest)
  // have special meaning for 0 IDs.
  if (next_id == 0)
    next_id = 1;
  args.GetReturnValue().Set(next_id);
}

}  // namespace extensions
