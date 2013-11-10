// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_ARRAY_BUFFER_H_
#define GIN_ARRAY_BUFFER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"  // For scoped_refptr only!
#include "v8/include/v8.h"

namespace gin {

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) OVERRIDE;
  virtual void* AllocateUninitialized(size_t length) OVERRIDE;
  virtual void Free(void* data, size_t length) OVERRIDE;

  static ArrayBufferAllocator* SharedInstance();
};

class BufferView {
 public:
  BufferView(v8::Isolate* isolate, v8::Handle<v8::ArrayBufferView> view);
  BufferView(v8::Isolate* isolate, v8::Handle<v8::ArrayBuffer> buffer);
  ~BufferView();

  void* bytes() const { return bytes_; }
  size_t num_bytes() const { return num_bytes_; }

 private:
  class Private;

  void Initialize(v8::Isolate* isolate, v8::Handle<v8::ArrayBuffer> buffer);

  scoped_refptr<Private> private_;
  void* bytes_;
  size_t num_bytes_;
};

}  // namespace gin

#endif  // GIN_ARRAY_BUFFER_H_
