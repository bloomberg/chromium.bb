// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/test/v8_test.h"

using v8::Context;
using v8::Local;
using v8::HandleScope;
using v8::Isolate;

namespace gin {

V8Test::V8Test() {
}

V8Test::~V8Test() {
}

void V8Test::SetUp() {
  isolate_ = Isolate::GetCurrent();
  HandleScope handle_scope(isolate_);
  context_.Reset(isolate_, Context::New(isolate_));
  Local<Context>::New(isolate_, context_)->Enter();
}

void V8Test::TearDown() {
  HandleScope handle_scope(isolate_);
  Local<Context>::New(isolate_, context_)->Exit();
  context_.Reset();
}

}  // namespace gin
