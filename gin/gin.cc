// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/gin.h"

#include <stdlib.h>
#include <string.h>

#include "base/rand_util.h"
#include "base/sys_info.h"
#include "gin/array_buffer.h"
#include "gin/per_isolate_data.h"

namespace gin {

namespace {

bool GenerateEntropy(unsigned char* buffer, size_t amount) {
  base::RandBytes(buffer, amount);
  return true;
}


void EnsureV8Initialized() {
  static bool v8_is_initialized = false;
  if (v8_is_initialized)
    return;
  v8_is_initialized = true;

  v8::V8::SetArrayBufferAllocator(ArrayBufferAllocator::SharedInstance());
  static const char v8_flags[] = "--use_strict --harmony";
  v8::V8::SetFlagsFromString(v8_flags, sizeof(v8_flags) - 1);
  v8::V8::SetEntropySource(&GenerateEntropy);
  v8::V8::Initialize();
}

}  // namespace

Gin::Gin() {
  EnsureV8Initialized();
  isolate_ = v8::Isolate::New();
  v8::ResourceConstraints constraints;
  constraints.ConfigureDefaults(base::SysInfo::AmountOfPhysicalMemory());
  v8::SetResourceConstraints(isolate_, &constraints);
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  new PerIsolateData(isolate_);
}

Gin::~Gin() {
  isolate_->Dispose();
}

}  // namespace gin
