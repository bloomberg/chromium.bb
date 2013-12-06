// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/wrappable.h"

#include "base/logging.h"
#include "gin/per_isolate_data.h"

namespace gin {

Wrappable::Wrappable() {
}

Wrappable::~Wrappable() {
  wrapper_.Reset();
}

v8::Handle<v8::Object> Wrappable::GetWrapper(v8::Isolate* isolate) {
  v8::Handle<v8::Value> wrapper = ConvertToV8(isolate, this);
  DCHECK(wrapper->IsObject());
  return v8::Handle<v8::Object>::Cast(wrapper);
}

void Wrappable::WeakCallback(
    const v8::WeakCallbackData<v8::Object, Wrappable>& data) {
  Wrappable* wrappable = data.GetParameter();
  wrappable->wrapper_.Reset();
  delete wrappable;
}

v8::Handle<v8::Object> Wrappable::CreateWrapper(v8::Isolate* isolate) {
  WrapperInfo* info = GetWrapperInfo();
  PerIsolateData* data = PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(info);
  CHECK(!templ.IsEmpty());  // Don't forget to register an object template.
  CHECK_EQ(kNumberOfInternalFields, templ->InternalFieldCount());
  v8::Handle<v8::Object> wrapper = templ->NewInstance();
  wrapper->SetAlignedPointerInInternalField(kWrapperInfoIndex, info);
  wrapper->SetAlignedPointerInInternalField(kEncodedValueIndex, this);
  wrapper_.Reset(isolate, wrapper);
  wrapper_.SetWeak(this, WeakCallback);
  return wrapper;
}

v8::Handle<v8::Value> Converter<Wrappable*>::ToV8(v8::Isolate* isolate,
                                                  Wrappable* val) {
  if (val->wrapper_.IsEmpty())
    return val->CreateWrapper(isolate);
  return v8::Local<v8::Object>::New(isolate, val->wrapper_);
}

bool Converter<Wrappable*>::FromV8(v8::Isolate* isolate,
                                   v8::Handle<v8::Value> val, Wrappable** out) {
  if (!val->IsObject())
    return false;
  v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(val);
  WrapperInfo* info = WrapperInfo::From(obj);
  if (!info)
    return false;
  void* pointer = obj->GetAlignedPointerFromInternalField(kEncodedValueIndex);
  Wrappable* wrappable = static_cast<Wrappable*>(pointer);
  CHECK(wrappable->GetWrapperInfo() == info);  // Security check for cast above.
  *out = wrappable;
  return true;
}

}  // namespace gin
