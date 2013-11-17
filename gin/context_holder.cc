// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/context_holder.h"

#include <assert.h>
#include "gin/per_context_data.h"

namespace gin {

ContextHolder::ContextHolder(v8::Isolate* isolate)
    : isolate_(isolate) {
}

ContextHolder::~ContextHolder() {
  v8::HandleScope handle_scope(isolate());
  v8::Handle<v8::Context> context = this->context();

  PerContextData* data = PerContextData::From(context);
  data->Detach(context);
  delete data;

  // TODO(abarth): Figure out how to set kResetInDestructor to true.
  context_.Reset();
}

void ContextHolder::SetContext(v8::Handle<v8::Context> context) {
  assert(context_.IsEmpty());
  context_.Reset(isolate_, context);
  new PerContextData(context);  // Deleted in ~ContextHolder.
}

}  // namespace gin
