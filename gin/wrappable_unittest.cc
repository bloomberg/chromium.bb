// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "gin/try_catch.h"
#include "gin/wrappable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gin {

class MyObject : public Wrappable<MyObject> {
 public:
  static WrapperInfo kWrapperInfo;

  static gin::Handle<MyObject> Create(v8::Isolate* isolate) {
    return CreateHandle(isolate, new MyObject());
  }

  int value() const { return value_; }
  void set_value(int value) { value_ = value; }

 private:
  MyObject() : value_(0) {}
  virtual ~MyObject() {}

  int value_;
};

class MyObject2 : public Wrappable<MyObject2> {
 public:
  static WrapperInfo kWrapperInfo;
};

class MyObjectBlink : public Wrappable<MyObjectBlink> {
 public:
  static WrapperInfo kWrapperInfo;
};

WrapperInfo MyObject::kWrapperInfo = { kEmbedderNativeGin };
WrapperInfo MyObject2::kWrapperInfo = { kEmbedderNativeGin };
WrapperInfo MyObjectBlink::kWrapperInfo = { kEmbedderNativeGin };

void RegisterTemplates(v8::Isolate* isolate) {
  PerIsolateData* data = PerIsolateData::From(isolate);
  DCHECK(data->GetObjectTemplate(&MyObject::kWrapperInfo).IsEmpty());

  v8::Handle<v8::ObjectTemplate> templ = ObjectTemplateBuilder(isolate)
      .SetProperty("value", &MyObject::value, &MyObject::set_value)
      .Build();
  templ->SetInternalFieldCount(kNumberOfInternalFields);
  data->SetObjectTemplate(&MyObject::kWrapperInfo, templ);

  templ = v8::ObjectTemplate::New(isolate);
  templ->SetInternalFieldCount(kNumberOfInternalFields);
  data->SetObjectTemplate(&MyObject2::kWrapperInfo, templ);

  templ = v8::ObjectTemplate::New(isolate);
  templ->SetInternalFieldCount(kNumberOfInternalFields);
  data->SetObjectTemplate(&MyObjectBlink::kWrapperInfo, templ);
}

typedef V8Test WrappableTest;

TEST_F(WrappableTest, WrapAndUnwrap) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  RegisterTemplates(isolate);
  Handle<MyObject> obj = MyObject::Create(isolate);

  v8::Handle<v8::Value> wrapper = ConvertToV8(isolate, obj.get());
  EXPECT_FALSE(wrapper.IsEmpty());

  MyObject* unwrapped = NULL;
  EXPECT_TRUE(ConvertFromV8(isolate, wrapper, &unwrapped));
  EXPECT_EQ(obj.get(), unwrapped);
}

TEST_F(WrappableTest, UnwrapFailures) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  RegisterTemplates(isolate);

  // Something that isn't an object.
  v8::Handle<v8::Value> thing = v8::Number::New(42);
  MyObject* unwrapped = NULL;
  EXPECT_FALSE(ConvertFromV8(isolate, thing, &unwrapped));
  EXPECT_FALSE(unwrapped);

  // An object that's not wrapping anything.
  thing = v8::Object::New(isolate);
  EXPECT_FALSE(ConvertFromV8(isolate, thing, &unwrapped));
  EXPECT_FALSE(unwrapped);

  // An object that's wrapping a C++ object from Blink.
  thing.Clear();
  thing = ConvertToV8(isolate, new MyObjectBlink());
  EXPECT_FALSE(ConvertFromV8(isolate, thing, &unwrapped));
  EXPECT_FALSE(unwrapped);

  // An object that's wrapping a C++ object of the wrong type.
  thing.Clear();
  thing = ConvertToV8(isolate, new MyObject2());
  EXPECT_FALSE(ConvertFromV8(isolate, thing, &unwrapped));
  EXPECT_FALSE(unwrapped);
}

TEST_F(WrappableTest, GetAndSetProperty) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  RegisterTemplates(isolate);
  gin::Handle<MyObject> obj = MyObject::Create(isolate);

  obj->set_value(42);
  EXPECT_EQ(42, obj->value());

  v8::Handle<v8::String> source = StringToV8(isolate,
      "(function (obj) {"
      "   if (obj.value !== 42) throw 'FAIL';"
      "   else obj.value = 191; })");
  EXPECT_FALSE(source.IsEmpty());

  gin::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::New(source);
  EXPECT_FALSE(script.IsEmpty());
  v8::Handle<v8::Value> val = script->Run();
  EXPECT_FALSE(val.IsEmpty());
  v8::Handle<v8::Function> func;
  EXPECT_TRUE(ConvertFromV8(isolate, val, &func));
  v8::Handle<v8::Value> argv[] = {
    ConvertToV8(isolate, obj.get()),
  };
  func->Call(v8::Undefined(isolate), 1, argv);
  EXPECT_FALSE(try_catch.HasCaught());
  EXPECT_EQ("", try_catch.GetStackTrace());

  EXPECT_EQ(191, obj->value());
}

}  // namespace gin
