// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/test/v8_test.h"

#include "gin/array_buffer.h"
#include "gin/public/isolate_holder.h"

using v8::Context;
using v8::Local;
using v8::HandleScope;

namespace gin {

V8Test::V8Test() {
}

V8Test::~V8Test() {
}

void V8Test::SetUp() {
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());
  instance_.reset(new gin::IsolateHolder);
  instance_->isolate()->Enter();
  HandleScope handle_scope(instance_->isolate());
  context_.Reset(instance_->isolate(), Context::New(instance_->isolate()));
  Local<Context>::New(instance_->isolate(), context_)->Enter();
}

void V8Test::TearDown() {
  {
    HandleScope handle_scope(instance_->isolate());
    Local<Context>::New(instance_->isolate(), context_)->Exit();
    context_.Reset();
  }
  instance_->isolate()->Exit();
  instance_.reset();
}

}  // namespace gin
