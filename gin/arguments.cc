// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/arguments.h"

#include <sstream>
#include "gin/converter.h"

namespace gin {

Arguments::Arguments(const v8::FunctionCallbackInfo<v8::Value>& info)
    : isolate_(info.GetIsolate()),
      info_(info),
      next_(0),
      insufficient_arguments_(false) {
}

Arguments::~Arguments() {
}

void Arguments::ThrowError() {
  if (insufficient_arguments_)
    return ThrowTypeError("Insufficient number of arguments.");

  std::stringstream stream;
  stream << "Error processing argument " << next_ - 1 << ".";
  ThrowTypeError(stream.str());
}

void Arguments::ThrowTypeError(const std::string& message) {
  isolate_->ThrowException(v8::Exception::TypeError(
      StringToV8(isolate_, message)));
}

}  // namespace gin
