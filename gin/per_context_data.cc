// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/per_context_data.h"

#include <assert.h>
#include "gin/wrapper_info.h"

namespace gin {

ContextSupplement::ContextSupplement() {
}

ContextSupplement::~ContextSupplement() {
}

PerContextData::PerContextData(v8::Handle<v8::Context> context) {
  context->SetAlignedPointerInEmbedderData(kEncodedValueIndex, this);
}

PerContextData::~PerContextData() {
  assert(supplements_.empty());
}

void PerContextData::Detach(v8::Handle<v8::Context> context) {
  assert(From(context) == this);
  context->SetAlignedPointerInEmbedderData(kEncodedValueIndex, NULL);

  SuplementVector supplements;
  supplements.swap(supplements_);

  for (SuplementVector::iterator it = supplements.begin();
       it != supplements.end(); ++it) {
    (*it)->Detach(context);
    delete *it;
  }
}

PerContextData* PerContextData::From(v8::Handle<v8::Context> context) {
  return static_cast<PerContextData*>(
      context->GetAlignedPointerFromEmbedderData(kEncodedValueIndex));
}

void PerContextData::AddSupplement(ContextSupplement* supplement) {
  supplements_.push_back(supplement);
}

}  // namespace gin
