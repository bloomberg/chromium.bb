// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_WRAPPABLE_H_
#define GIN_WRAPPABLE_H_

#include "base/memory/ref_counted.h"
#include "gin/converter.h"
#include "gin/public/wrapper_info.h"

namespace gin {

class Wrappable : public base::RefCounted<Wrappable> {
 public:
  virtual WrapperInfo* GetWrapperInfo() = 0;

  v8::Handle<v8::Object> GetWrapper(v8::Isolate* isolate);

 protected:
  Wrappable();
  virtual ~Wrappable();

 private:
  friend class base::RefCounted<Wrappable>;
  friend struct Converter<Wrappable*>;

  static void WeakCallback(
      const v8::WeakCallbackData<v8::Object, Wrappable>& data);
  v8::Handle<v8::Object> CreateWrapper(v8::Isolate* isolate);

  v8::Persistent<v8::Object> wrapper_;  // Weak

  DISALLOW_COPY_AND_ASSIGN(Wrappable);
};

template<>
struct Converter<Wrappable*> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    Wrappable* val);
  static bool FromV8(v8::Handle<v8::Value> val,
                     Wrappable** out);
};

template<typename T>
struct WrappableConverter {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate, T* val) {
    return Converter<Wrappable*>::ToV8(isolate, val);
  }
  static bool FromV8(v8::Handle<v8::Value> val, T** out) {
    Wrappable* wrappable = 0;
    if (!Converter<Wrappable*>::FromV8(val, &wrappable)
        || wrappable->GetWrapperInfo() != &T::kWrapperInfo)
      return false;
    *out = static_cast<T*>(wrappable);
    return true;
  }
};

}  // namespace gin

#endif  // GIN_WRAPPABLE_H_
