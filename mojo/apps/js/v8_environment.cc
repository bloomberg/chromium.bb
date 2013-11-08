// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/v8_environment.h"

#include <stdlib.h>
#include <string.h>

#include "mojo/public/system/macros.h"
#include "v8/include/v8.h"

namespace mojo {
namespace apps {

namespace {

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
  virtual void* Allocate(size_t length) MOJO_OVERRIDE {
    return calloc(1, length);
  }
  virtual void* AllocateUninitialized(size_t length) MOJO_OVERRIDE {
    return malloc(length);
  }
  virtual void Free(void* data, size_t length) MOJO_OVERRIDE {
    free(data);
  }
};

bool GenerateEntropy(unsigned char* buffer, size_t amount) {
  // TODO(abarth): Mojo needs a source of entropy.
  // https://code.google.com/p/chromium/issues/detail?id=316387
  return false;
}

const char kFlags[] = "--use_strict --harmony";

}

void InitializeV8() {
  v8::V8::SetArrayBufferAllocator(new ArrayBufferAllocator());
  v8::V8::InitializeICU();
  v8::V8::SetFlagsFromString(kFlags, strlen(kFlags));
  v8::V8::SetEntropySource(&GenerateEntropy);
  v8::V8::Initialize();
}

}  // namespace apps
}  // mojo
