// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_OBJECT_TEMPLATE_BUILDER_H_
#define GIN_OBJECT_TEMPLATE_BUILDER_H_

#include <type_traits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "gin/gin_export.h"
#include "v8/include/v8.h"

namespace gin {

template <typename T>
v8::Local<v8::FunctionTemplate> CreateFunctionTemplate(v8::Isolate* isolate,
                                                       T callback) {
  // We need to handle member function pointers case specially because the first
  // parameter for callbacks to MFP should typically come from the the
  // JavaScript "this" object the function was called on, not from the first
  // normal parameter.
  int callback_flags = 0;
  if (std::is_member_function_pointer<T>::value)
    callback_flags = HolderIsFirstArgument;

  return CreateFunctionTemplate(
      isolate, base::BindRepeating(std::move(callback)), callback_flags);
}

template <typename T>
void SetAsFunctionHandler(v8::Isolate* isolate,
                          v8::Local<v8::ObjectTemplate> tmpl,
                          T callback) {
  // We need to handle member function pointers case specially because the first
  // parameter for callbacks to MFP should typically come from the the
  // JavaScript "this" object the function was called on, not from the first
  // normal parameter.
  int callback_flags = 0;
  if (std::is_member_function_pointer<T>::value)
    callback_flags = HolderIsFirstArgument;

  CreateFunctionHandler(isolate, tmpl, base::BindRepeating(std::move(callback)),
                        callback_flags);
}

// ObjectTemplateBuilder provides a handy interface to creating
// v8::ObjectTemplate instances with various sorts of properties.
class GIN_EXPORT ObjectTemplateBuilder {
 public:
  explicit ObjectTemplateBuilder(v8::Isolate* isolate);
  ObjectTemplateBuilder(const ObjectTemplateBuilder& other);
  ~ObjectTemplateBuilder();

  // It's against Google C++ style to return a non-const ref, but we take some
  // poetic license here in order that all calls to Set() can be via the '.'
  // operator and line up nicely.
  template<typename T>
  ObjectTemplateBuilder& SetValue(const base::StringPiece& name, T val) {
    return SetImpl(name, ConvertToV8(isolate_, val));
  }

  // In the following methods, T and U can be function pointer, member function
  // pointer, base::Callback, or v8::FunctionTemplate. Most clients will want to
  // use one of the first two options. Also see gin::CreateFunctionTemplate()
  // for creating raw function templates.
  template<typename T>
  ObjectTemplateBuilder& SetMethod(const base::StringPiece& name,
                                   const T& callback) {
    return SetImpl(name, CreateFunctionTemplate(isolate_, callback));
  }
  template<typename T>
  ObjectTemplateBuilder& SetProperty(const base::StringPiece& name,
                                     const T& getter) {
    return SetPropertyImpl(name, CreateFunctionTemplate(isolate_, getter),
                           v8::Local<v8::FunctionTemplate>());
  }
  template<typename T, typename U>
  ObjectTemplateBuilder& SetProperty(const base::StringPiece& name,
                                     const T& getter, const U& setter) {
    return SetPropertyImpl(name, CreateFunctionTemplate(isolate_, getter),
                           CreateFunctionTemplate(isolate_, setter));
  }
  template<typename T>
  ObjectTemplateBuilder& SetCallAsFunctionHandler(const T& callback) {
    SetAsFunctionHandler(isolate_, template_, callback);
    return *this;
  }
  ObjectTemplateBuilder& AddNamedPropertyInterceptor();
  ObjectTemplateBuilder& AddIndexedPropertyInterceptor();

  v8::Local<v8::ObjectTemplate> Build();

 private:
  ObjectTemplateBuilder& SetImpl(const base::StringPiece& name,
                                 v8::Local<v8::Data> val);
  ObjectTemplateBuilder& SetPropertyImpl(
      const base::StringPiece& name, v8::Local<v8::FunctionTemplate> getter,
      v8::Local<v8::FunctionTemplate> setter);

  v8::Isolate* isolate_;

  // ObjectTemplateBuilder should only be used on the stack.
  v8::Local<v8::ObjectTemplate> template_;
};

}  // namespace gin

#endif  // GIN_OBJECT_TEMPLATE_BUILDER_H_
