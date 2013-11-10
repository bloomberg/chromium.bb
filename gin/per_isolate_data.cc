// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/per_isolate_data.h"

using v8::Eternal;
using v8::Handle;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::ObjectTemplate;

namespace gin {

PerIsolateData::PerIsolateData(Isolate* isolate)
    : isolate_(isolate)  {
  isolate_->SetData(this);
}

PerIsolateData::~PerIsolateData() {
}

PerIsolateData* PerIsolateData::From(Isolate* isolate) {
  return static_cast<PerIsolateData*>(isolate->GetData());
}

void PerIsolateData::RegisterObjectTemplate(
    WrapperInfo* info, Local<ObjectTemplate> object_template) {
  object_templates_[info] = Eternal<ObjectTemplate>(isolate_, object_template);
}

}  // namespace gin
