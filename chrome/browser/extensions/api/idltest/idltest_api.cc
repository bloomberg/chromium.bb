// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/idltest/idltest_api.h"

#include "base/values.h"

using base::BinaryValue;

namespace {

ListValue* CopyBinaryValueToIntegerList(const BinaryValue* input) {
  ListValue* output = new ListValue();
  const char* input_buffer = input->GetBuffer();
  for (size_t i = 0; i < input->GetSize(); i++) {
    output->Append(Value::CreateIntegerValue(input_buffer[i]));
  }
  return output;
}

}

bool IdltestSendArrayBufferFunction::RunImpl() {
  BinaryValue* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_ != NULL && args_->GetBinary(0, &input));
  result_.reset(CopyBinaryValueToIntegerList(input));
  return true;
}

bool IdltestSendArrayBufferViewFunction::RunImpl() {
  BinaryValue* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_ != NULL && args_->GetBinary(0, &input));
  result_.reset(CopyBinaryValueToIntegerList(input));
  return true;
}

bool IdltestGetArrayBufferFunction::RunImpl() {
  std::string hello = "hello world";
  BinaryValue* output =
      BinaryValue::CreateWithCopiedBuffer(hello.c_str(), hello.size());
  result_.reset(output);
  return true;
}
