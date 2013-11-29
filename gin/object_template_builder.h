// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_OBJECT_TEMPLATE_BUILDER_H_
#define GIN_OBJECT_TEMPLATE_BUILDER_H_

#include "base/bind.h"
#include "base/strings/string_piece.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "v8/include/v8.h"

namespace gin {

// ObjectTemplateBuilder provides a handy interface to creating
// v8::ObjectTemplate instances with various sorts of properties.
class ObjectTemplateBuilder {
 public:
  explicit ObjectTemplateBuilder(v8::Isolate* isolate);
  ~ObjectTemplateBuilder();

  // It's against Google C++ style to return a non-const ref, but we take some
  // poetic license here in order that all calls to Set() can be via the '.'
  // operator and line up nicely.
  template<typename T>
  ObjectTemplateBuilder& SetValue(const base::StringPiece& name, T val) {
    return SetImpl(name, ConvertToV8(isolate_, val));
  }

  template<typename T>
  ObjectTemplateBuilder& SetMethod(const base::StringPiece& name, T val) {
    return SetMethod(name, base::Bind(val));
  }

  template<typename T>
  ObjectTemplateBuilder& SetMethod(const base::StringPiece& name,
                                   const base::Callback<T>& callback) {
    return SetImpl(name, CreateFunctionTemplate(isolate_, callback));
  }

  v8::Local<v8::ObjectTemplate> Build();

 private:
  ObjectTemplateBuilder& SetImpl(const base::StringPiece& name,
                                 v8::Handle<v8::Data> val);

  v8::Isolate* isolate_;

  // ObjectTemplateBuilder should only be used on the stack.
  v8::Local<v8::ObjectTemplate> template_;
};

}  // namespace gin

#endif  // GIN_OBJECT_TEMPLATE_BUILDER_H_
