// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_ISOLATE_HOLDER_H_
#define GIN_PUBLIC_ISOLATE_HOLDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "gin/gin_export.h"

namespace v8 {
class Isolate;
}

namespace gin {

class PerIsolateData;

// To embed Gin, first create an instance of IsolateHolder to hold the
// v8::Isolate in which you will execute JavaScript. You might wish to subclass
// IsolateHolder if you want to tie more state to the lifetime of the
//
// You can use gin in two modes: either gin manages V8, or the gin-embedder
// manages gin. If gin manages V8, use the IsolateHolder constructor without
// parameters, otherwise, the gin-embedder needs to create v8::Isolates and
// pass them to IsolateHolder.
//
// It is not possible to mix the two.
class GIN_EXPORT IsolateHolder {
 public:
  IsolateHolder();
  explicit IsolateHolder(v8::Isolate* isolate);

  ~IsolateHolder();

  v8::Isolate* isolate() { return isolate_; }

 private:
  void Init();

  bool isolate_owner_;
  v8::Isolate* isolate_;
  scoped_ptr<PerIsolateData> isolate_data_;

  DISALLOW_COPY_AND_ASSIGN(IsolateHolder);
};

}  // namespace gin

#endif  // GIN_PUBLIC_ISOLATE_HOLDER_H_
