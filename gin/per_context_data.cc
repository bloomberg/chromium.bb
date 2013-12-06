// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/per_context_data.h"

#include "base/logging.h"
#include "gin/public/context_holder.h"
#include "gin/public/wrapper_info.h"

namespace gin {

ContextSupplement::ContextSupplement() {
}

ContextSupplement::~ContextSupplement() {
}

PerContextData::PerContextData(v8::Handle<v8::Context> context)
    : runner_(NULL) {
  context->SetAlignedPointerInEmbedderData(
      kPerContextDataStartIndex + kEmbedderNativeGin, this);
}

PerContextData::~PerContextData() {
  DCHECK(supplements_.empty());
}

void PerContextData::Detach(v8::Handle<v8::Context> context) {
  DCHECK(From(context) == this);
  context->SetAlignedPointerInEmbedderData(
      kPerContextDataStartIndex + kEmbedderNativeGin, NULL);

  SuplementVector supplements;
  supplements.swap(supplements_);

  for (SuplementVector::iterator it = supplements.begin();
       it != supplements.end(); ++it) {
    (*it)->Detach(context);
  }
}

PerContextData* PerContextData::From(v8::Handle<v8::Context> context) {
  return static_cast<PerContextData*>(
      context->GetAlignedPointerFromEmbedderData(kEncodedValueIndex));
}

void PerContextData::AddSupplement(scoped_ptr<ContextSupplement> supplement) {
  supplements_.push_back(supplement.release());
}

}  // namespace gin
