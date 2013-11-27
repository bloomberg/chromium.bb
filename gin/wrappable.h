// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_WRAPPABLE_H_
#define GIN_WRAPPABLE_H_

#include "base/memory/ref_counted.h"
#include "gin/converter.h"
#include "gin/public/wrapper_info.h"

namespace gin {

// Wrappable is an abstract base class for C++ objects that have cooresponding
// v8 wrapper objects. Wrappable are RefCounted, which means they can be
// retained either by V8's garbage collector or by a scoped_refptr.
//
// WARNING: If you retain a Wrappable object with a scoped_refptr, it's possible
//          that the v8 wrapper can "fall off" if the wrapper object is not
//          referenced elsewhere in the V8 heap. Although Wrappable opens a
//          handle to the wrapper object, we make that handle as weak, which
//          means V8 is free to reclaim the wrapper. (We can't make the handle
//          strong without risking introducing a memory leak if an object that
//          holds a scoped_refptr is reachable from the wrapper.)
//
class Wrappable : public base::RefCounted<Wrappable> {
 public:
  // Subclasses must return the WrapperInfo object associated with the
  // v8::ObjectTemplate for their subclass. When creating a v8 wrapper for
  // this object, we'll look up the appropriate v8::ObjectTemplate in the
  // PerIsolateData using this WrapperInfo pointer.
  virtual WrapperInfo* GetWrapperInfo() = 0;

  // Subclasses much also contain a static member variable named |kWrapperInfo|
  // of type WrapperInfo:
  //
  //   static WrapperInfo kWrapperInfo;
  //
  // If |obj| is a concrete instance of the subclass, then obj->GetWrapperInfo()
  // must return &kWrapperInfo.
  //
  // We use both the dynamic |GetWrapperInfo| function and the static
  // |kWrapperInfo| member variable during wrapping and the unwrapping. During
  // wrapping, we use GetWrapperInfo() to make sure we use the correct
  // v8::ObjectTemplate for the object regardless of the declared C++ type.
  // During unwrapping, we use the static member variable to prevent type errors
  // during the downcast from Wrappable to the subclass.

  // Retrieve (or create) the v8 wrapper object cooresponding to this object.
  // To customize the wrapper created for a subclass, override GetWrapperInfo()
  // instead of overriding this function.
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
    // Currently we require that you unwrap to the exact runtime type of the
    // wrapped object.
    // TODO(abarth): Support unwrapping to a base class.
    *out = static_cast<T*>(wrappable);
    return true;
  }
};

}  // namespace gin

#endif  // GIN_WRAPPABLE_H_
