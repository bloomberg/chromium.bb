// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/per_context_data.h"

#include "base/logging.h"
#include "gin/public/context_holder.h"
#include "gin/public/wrapper_info.h"

namespace gin {

PerContextData::PerContextData(v8::Handle<v8::Context> context)
    : runner_(NULL) {
  context->SetAlignedPointerInEmbedderData(
      kPerContextDataStartIndex + kEmbedderNativeGin, this);
}

PerContextData::~PerContextData() {
}

// static
PerContextData* PerContextData::From(v8::Handle<v8::Context> context) {
  return static_cast<PerContextData*>(
      context->GetAlignedPointerFromEmbedderData(kEncodedValueIndex));
}

}  // namespace gin
