// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_WRAPPABLE_H_
#define GIN_WRAPPABLE_H_

#include "base/template_util.h"
#include "gin/converter.h"
#include "gin/public/wrapper_info.h"

namespace gin {

// Wrappable is an abstract base class for C++ objects that have cooresponding
// v8 wrapper objects. To retain a Wrappable object on the stack, use a
// gin::Handle.
class Wrappable {
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

  // Subclasses should have private constructors and expose a static Create
  // function that returns a gin::Handle. Forcing creators through this static
  // Create function will enforce that clients actually create a wrapper for
  // the object. If clients fail to create a wrapper for a wrappable object,
  // the object will leak because we use the weak callback from the wrapper
  // as the signal to delete the wrapped object.

 protected:
  Wrappable();
  virtual ~Wrappable();

 private:
  static void WeakCallback(
      const v8::WeakCallbackData<v8::Object, Wrappable>& data);
  v8::Handle<v8::Object> CreateWrapper(v8::Isolate* isolate);

  v8::Persistent<v8::Object> wrapper_;  // Weak

  DISALLOW_COPY_AND_ASSIGN(Wrappable);
};

// This converter handles any subclass of Wrappable.
template<typename T>
struct Converter<T*, typename base::enable_if<
                      base::is_convertible<T*, Wrappable*>::value>::type> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate, T* val) {
    return val->GetWrapper(isolate);
  }

  static bool FromV8(v8::Isolate* isolate, v8::Handle<v8::Value> val, T** out) {
    if (!val->IsObject())
      return false;
    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(val);
    WrapperInfo* info = WrapperInfo::From(obj);

    // If this fails, the object is not managed by Gin. It is either a normal JS
    // object that's not wrapping any external C++ object, or it is wrapping
    // some C++ object, but that object isn't managed by Gin (maybe Blink).
    if (!info)
      return false;

    // If this fails, the object is managed by Gin, but it's not wrapping an
    // instance of T.
    if (info != &T::kWrapperInfo)
      return false;

    void* pointer = obj->GetAlignedPointerFromInternalField(kEncodedValueIndex);
    T* result = static_cast<T*>(pointer);

    // If this fails, something fishy is going on. |info| should have come from
    // T::GetWrapperInfo(), but somehow is now different than it. So panic.
    CHECK(result->GetWrapperInfo() == info);
    *out = result;
    return true;
  }
};

}  // namespace gin

#endif  // GIN_WRAPPABLE_H_
