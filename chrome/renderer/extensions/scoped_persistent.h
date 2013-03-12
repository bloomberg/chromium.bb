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
    reset();
    handle_ = v8::Persistent<T>::New(GetIsolate(handle), handle);
  }

  void reset() {
    if (handle_.IsEmpty())
      return;
    handle_.Dispose(GetIsolate(handle_));
    handle_.Clear();
  }

  v8::Persistent<T> operator->() const {
    return handle_;
  }

  v8::Persistent<T> get() const {
    return handle_;
  }

  void MakeWeak(void* parameters, v8::NearDeathCallback callback) {
    handle_.MakeWeak(GetIsolate(handle_), parameters, callback);
  }

 private:
  template <typename U>
  static v8::Isolate* GetIsolate(v8::Handle<U> object_handle) {
    // Only works for v8::Object and its subclasses. Add specialisations for
    // anything else.
    return GetIsolate(object_handle->CreationContext());
  }
  static v8::Isolate* GetIsolate(v8::Handle<v8::Context> context_handle) {
    return context_handle->GetIsolate();
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
