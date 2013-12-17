// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_WRAPPABLE_H_
#define GIN_WRAPPABLE_H_

#include "base/template_util.h"
#include "gin/converter.h"
#include "gin/gin_export.h"
#include "gin/public/wrapper_info.h"

namespace gin {

namespace internal {

GIN_EXPORT void* FromV8Impl(v8::Isolate* isolate,
                            v8::Handle<v8::Value> val,
                            WrapperInfo* info);

}  // namespace internal


// Wrappable is a base class for C++ objects that have corresponding v8 wrapper
// objects. To retain a Wrappable object on the stack, use a gin::Handle.
//
// USAGE:
// // my_class.h
// class MyClass : Wrappable<MyClass> {
//  public:
//   static WrapperInfo kWrapperInfo;
//
//   // Optional, only required if non-empty template should be used.
//   static v8::Local<v8::ObjectTemplate> GetObjectTemplate(
//       v8::Isolate* isolate);
//   ...
// };
//
// // my_class.cc
// WrapperInfo MyClass::kWrapperInfo = { kEmbedderNativeGin };
//
// v8::Local<v8::ObjectTemplate> MyClass::GetObjectTemplate(
//     v8::Isolate* isolate) {
//   return ObjectTemplateBuilder(isolate).SetValue("foobar", 42).Build();
// }
//
// Subclasses should also typically have private constructors and expose a
// static Create function that returns a gin::Handle. Forcing creators through
// this static Create function will enforce that clients actually create a
// wrapper for the object. If clients fail to create a wrapper for a wrappable
// object, the object will leak because we use the weak callback from the
// wrapper as the signal to delete the wrapped object.
template<typename T>
class Wrappable;


// Non-template base class to share code between templates instances.
class GIN_EXPORT WrappableBase {
 protected:
  typedef v8::Local<v8::ObjectTemplate>(*GetObjectTemplateFunction)(
      v8::Isolate*);

  WrappableBase();
  virtual ~WrappableBase();

  v8::Handle<v8::Object> GetWrapperImpl(
      v8::Isolate* isolate,
      WrapperInfo* wrapper_info,
      GetObjectTemplateFunction template_getter);

  static v8::Local<v8::ObjectTemplate> GetObjectTemplate(v8::Isolate* isolate);

 private:
  static void WeakCallback(
      const v8::WeakCallbackData<v8::Object, WrappableBase>& data);

  v8::Handle<v8::Object> CreateWrapper(
      v8::Isolate* isolate,
      WrapperInfo* wrapper_info,
      GetObjectTemplateFunction template_getter);

  v8::Persistent<v8::Object> wrapper_;  // Weak

  DISALLOW_COPY_AND_ASSIGN(WrappableBase);
};


template<typename T>
class Wrappable : public WrappableBase {
 public:
  // Retrieve (or create) the v8 wrapper object cooresponding to this object.
  // To customize the wrapper created for a subclass, override GetWrapperInfo()
  // instead of overriding this function.
  v8::Handle<v8::Object> GetWrapper(v8::Isolate* isolate) {
    return GetWrapperImpl(isolate, &T::kWrapperInfo, &T::GetObjectTemplate);
  }

 protected:
  Wrappable() {}
  virtual ~Wrappable() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Wrappable);
};


// This converter handles any subclass of Wrappable.
template<typename T>
struct Converter<T*, typename base::enable_if<
                       base::is_convertible<T*, Wrappable<T>*>::value>::type> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate, T* val) {
    return val->GetWrapper(isolate);
  }

  static bool FromV8(v8::Isolate* isolate, v8::Handle<v8::Value> val, T** out) {
    *out = static_cast<T*>(internal::FromV8Impl(isolate, val,
                                                &T::kWrapperInfo));
    return *out != NULL;
  }
};

}  // namespace gin

#endif  // GIN_WRAPPABLE_H_
