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

namespace internal {

template <typename T>
v8::Local<v8::FunctionTemplate> CreateFunctionTemplate(v8::Isolate* isolate,
                                                       T callback,
                                                       const char* type_name) {
  // We need to handle member function pointers case specially because the first
  // parameter for callbacks to MFP should typically come from the the
  // JavaScript "this" object the function was called on, not from the first
  // normal parameter.
  InvokerOptions options;
  if (std::is_member_function_pointer<T>::value) {
    options.holder_is_first_argument = true;
    options.holder_type = type_name;
  }
  return ::gin::CreateFunctionTemplate(
      isolate, base::BindRepeating(std::move(callback)), std::move(options));
}

}  // namespace internal

template <typename T>
void SetAsFunctionHandler(v8::Isolate* isolate,
                          v8::Local<v8::ObjectTemplate> tmpl,
                          T callback) {
  // We need to handle member function pointers case specially because the first
  // parameter for callbacks to MFP should typically come from the the
  // JavaScript "this" object the function was called on, not from the first
  // normal parameter.
  InvokerOptions options = {std::is_member_function_pointer<T>::value, nullptr};

  CreateFunctionHandler(isolate, tmpl, base::BindRepeating(std::move(callback)),
                        std::move(options));
}

// ObjectTemplateBuilder provides a handy interface to creating
// v8::ObjectTemplate instances with various sorts of properties.
class GIN_EXPORT ObjectTemplateBuilder {
 public:
  explicit ObjectTemplateBuilder(v8::Isolate* isolate);
  ObjectTemplateBuilder(v8::Isolate* isolate, const char* type_name);
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
    return SetImpl(
        name, internal::CreateFunctionTemplate(isolate_, callback, type_name_));
  }
  template<typename T>
  ObjectTemplateBuilder& SetProperty(const base::StringPiece& name,
                                     const T& getter) {
    return SetPropertyImpl(
        name, internal::CreateFunctionTemplate(isolate_, getter, type_name_),
        v8::Local<v8::FunctionTemplate>());
  }
  template<typename T, typename U>
  ObjectTemplateBuilder& SetProperty(const base::StringPiece& name,
                                     const T& getter, const U& setter) {
    return SetPropertyImpl(
        name, internal::CreateFunctionTemplate(isolate_, getter, type_name_),
        internal::CreateFunctionTemplate(isolate_, setter, type_name_));
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

  // If provided, |type_name_| will be used to give a user-friendly error
  // message if a member function is invoked on the wrong type of object.
  const char* type_name_ = nullptr;

  // ObjectTemplateBuilder should only be used on the stack.
  v8::Local<v8::ObjectTemplate> template_;
};

}  // namespace gin

#endif  // GIN_OBJECT_TEMPLATE_BUILDER_H_
