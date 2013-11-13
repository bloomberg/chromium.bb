// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/initialize.h"

#include <stdlib.h>
#include <string.h>

#include "gin/array_buffer.h"
#include "gin/per_isolate_data.h"

namespace gin {

namespace {

bool GenerateEntropy(unsigned char* buffer, size_t amount) {
  // TODO(abarth): Gin needs a source of entropy.
  return false;
}

const char kFlags[] = "--use_strict --harmony";

}

void Initialize() {
  v8::V8::SetArrayBufferAllocator(ArrayBufferAllocator::SharedInstance());
  v8::V8::InitializeICU();
  v8::V8::SetFlagsFromString(kFlags,
      static_cast<uint32_t>(sizeof(kFlags) / sizeof(kFlags[0])) - 1);
  v8::V8::SetEntropySource(&GenerateEntropy);
  v8::V8::Initialize();

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  new PerIsolateData(isolate);
}

}  // namespace gin
