// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/idltest/idltest_api.h"

#include <stddef.h>

#include <memory>

#include "base/values.h"

using base::Value;

namespace {

std::unique_ptr<base::ListValue> CopyBinaryValueToIntegerList(
    const Value* input) {
  std::unique_ptr<base::ListValue> output(new base::ListValue());
  for (char c : input->GetBlob()) {
    output->AppendInteger(c);
  }
  return output;
}

}  // namespace

ExtensionFunction::ResponseAction IdltestSendArrayBufferFunction::Run() {
  Value* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_ != NULL && args_->GetBinary(0, &input));
  return RespondNow(OneArgument(CopyBinaryValueToIntegerList(input)));
}

ExtensionFunction::ResponseAction IdltestSendArrayBufferViewFunction::Run() {
  Value* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_ != NULL && args_->GetBinary(0, &input));
  return RespondNow(OneArgument(CopyBinaryValueToIntegerList(input)));
}

ExtensionFunction::ResponseAction IdltestGetArrayBufferFunction::Run() {
  std::string hello = "hello world";
  return RespondNow(
      OneArgument(Value::CreateWithCopiedBuffer(hello.c_str(), hello.size())));
}
