// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_ISOLATE_HOLDER_H_
#define GIN_PUBLIC_ISOLATE_HOLDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "gin/gin_export.h"
#include "v8/include/v8.h"

namespace gin {

class PerIsolateData;

// To embed Gin, first initialize gin using IsolateHolder::Initialize and then
// create an instance of IsolateHolder to hold the v8::Isolate in which you
// will execute JavaScript. You might wish to subclass IsolateHolder if you
// want to tie more state to the lifetime of the isolate.
class GIN_EXPORT IsolateHolder {
 public:
  // Controls whether or not V8 should only accept strict mode scripts.
  enum ScriptMode {
    kNonStrictMode,
    kStrictMode
  };

  IsolateHolder();
  ~IsolateHolder();

  // Should be invoked once before creating IsolateHolder instances to
  // initialize V8 and Gin.
  static void Initialize(ScriptMode mode,
                         v8::ArrayBuffer::Allocator* allocator);

  v8::Isolate* isolate() { return isolate_; }

 private:
  v8::Isolate* isolate_;
  scoped_ptr<PerIsolateData> isolate_data_;

  DISALLOW_COPY_AND_ASSIGN(IsolateHolder);
};

}  // namespace gin

#endif  // GIN_PUBLIC_ISOLATE_HOLDER_H_
