// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ALLOCATION_SCOPE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ALLOCATION_SCOPE_H_

#include "mojo/public/cpp/bindings/lib/scratch_buffer.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

// In order to allocate a Mojom-defined structure or mojo::Array<T> (including
// mojo::String), an AllocationScope must first be allocated. Typically,
// AllocationScope is placed on the stack before calls to build structs and
// arrays. Such structs and arrays are valid so long as the corresponding
// AllocationScope remains alive.
//
// AllocationScope instantiates a Buffer and sets it in thread local storage.
// This Buffer instance can be retrieved using Buffer::current().
//
// EXAMPLE:
//
//   mojo::AllocationScope scope;
//   mojo::String s = "hello world";
//   some_interface->SomeMethod(s);
//
class AllocationScope {
 public:
  AllocationScope() {}
  ~AllocationScope() {}

  Buffer* buffer() { return &buffer_; }

 private:
  internal::ScratchBuffer buffer_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(AllocationScope);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ALLOCATION_SCOPE_H_
