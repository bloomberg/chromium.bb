// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SYSTEM_MACROS_H_
#define MOJO_PUBLIC_SYSTEM_MACROS_H_

#ifdef __cplusplus

namespace mojo {

// Annotate a virtual method indicating it must be overriding a virtual
// method in the parent class.
// Use like:
//   virtual void foo() OVERRIDE;
#if defined(_MSC_VER) || defined(__clang__)
#define MOJO_OVERRIDE override
#else
#define MOJO_OVERRIDE
#endif

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define MOJO_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);                    \
  void operator=(const TypeName&)

// Used to assert things at compile time.
namespace internal { template <bool> struct CompileAssert {}; }
#define MOJO_COMPILE_ASSERT(expr, msg) \
    typedef internal::CompileAssert<(bool(expr))> msg[bool(expr) ? 1 : -1]

}  // namespace mojo

#endif  // __cplusplus

#endif  // MOJO_PUBLIC_SYSTEM_MACROS_H_
