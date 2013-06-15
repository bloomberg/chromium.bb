// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_SCOPED_PERSISTENT_H_
#define CHROME_RENDERER_EXTENSIONS_SCOPED_PERSISTENT_H_

#include "base/logging.h"
#include "v8/include/v8.h"

namespace extensions {

// A v8::Persistent handle to a V8 value which destroys and clears the
// underlying handle on destruction.
template <typename T>
class ScopedPersistent {
 public:
  ScopedPersistent() {
  }

  explicit ScopedPersistent(v8::Handle<T> handle) {
    reset(handle);
  }

  ~ScopedPersistent() {
    reset();
  }

  void reset(v8::Handle<T> handle) {
    if (!handle.IsEmpty())
      handle_.Reset(GetIsolate(handle), handle);
    else
      reset();
  }

  void reset() {
    if (handle_.IsEmpty())
      return;
    handle_.Dispose();
    handle_.Clear();
  }

  v8::Handle<T> operator->() const {
    return get();
  }

  // TODO(dcarney): Remove this function
  // This is an unsafe access to the underlying handle
  v8::Handle<T> get() const {
    return *reinterpret_cast<v8::Handle<T>*>(
      const_cast<v8::Persistent<T>* >(&handle_));
  }

  v8::Handle<T> NewHandle() const {
    if (handle_.IsEmpty())
      return v8::Local<T>();
    return v8::Local<T>::New(GetIsolate(handle_), handle_);
  }

  template<typename P>
  void MakeWeak(P* parameters,
                typename v8::WeakReferenceCallbacks<T, P>::Revivable callback) {
    handle_.MakeWeak(parameters, callback);
  }

 private:
  template <typename U>
  static v8::Isolate* GetIsolate(v8::Handle<U> object_handle) {
    // Only works for v8::Object and its subclasses. Add specialisations for
    // anything else.
    if (!object_handle.IsEmpty())
      return GetIsolate(object_handle->CreationContext());
    return v8::Isolate::GetCurrent();
  }
  static v8::Isolate* GetIsolate(v8::Handle<v8::Context> context_handle) {
    if (!context_handle.IsEmpty())
      return context_handle->GetIsolate();
    return v8::Isolate::GetCurrent();
  }
  static v8::Isolate* GetIsolate(
      v8::Handle<v8::ObjectTemplate> template_handle) {
    return v8::Isolate::GetCurrent();
  }

  v8::Persistent<T> handle_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPersistent);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_SCOPED_PERSISTENT_H_
