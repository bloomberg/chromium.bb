// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_ARGUMENTS_H_
#define GIN_ARGUMENTS_H_

#include "base/basictypes.h"
#include "gin/converter.h"

namespace gin {

class Arguments {
 public:
  explicit Arguments(const v8::FunctionCallbackInfo<v8::Value>& info);
  ~Arguments();

  template<typename T>
  bool Holder(T* out) {
    return ConvertFromV8(info_.Holder(), out);
  }

  template<typename T>
  bool GetNext(T* out) {
    if (next_ >= info_.Length()) {
      insufficient_arguments_ = true;
      return false;
    }
    v8::Handle<v8::Value> val = info_[next_++];
    return ConvertFromV8(val, out);
  }

  template<typename T>
  void Return(T val) {
    info_.GetReturnValue().Set(ConvertToV8(isolate_, val));
  }

  void ThrowError();
  void ThrowTypeError(const std::string& message);

  v8::Isolate* isolate() const { return isolate_; }

 private:
  v8::Isolate* isolate_;
  const v8::FunctionCallbackInfo<v8::Value>& info_;
  int next_;
  bool insufficient_arguments_;

  DISALLOW_COPY_AND_ASSIGN(Arguments);
};

}  // namespace gin

#endif  // GIN_ARGUMENTS_H_
