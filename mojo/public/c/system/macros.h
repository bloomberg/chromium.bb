// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_C_SYSTEM_MACROS_H_
#define MOJO_PUBLIC_C_SYSTEM_MACROS_H_

#include <stddef.h>

// Annotate a variable indicating it's okay if it's unused.
// Use like:
//   int x MOJO_ALLOW_UNUSED = ...;
#if defined(__GNUC__)
#define MOJO_ALLOW_UNUSED __attribute__((unused))
#else
#define MOJO_ALLOW_UNUSED
#endif

// Annotate a function indicating that the caller must examine the return value.
// Use like:
//   int foo() MOJO_WARN_UNUSED_RESULT;
// Note that it can only be used on the prototype, and not the definition.
#if defined(__GNUC__)
#define MOJO_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define MOJO_WARN_UNUSED_RESULT
#endif

// This macro is currently C++-only, but we want to use it in the C core.h.
#ifdef __cplusplus
// Used to assert things at compile time.
namespace mojo { template <bool> struct CompileAssert {}; }
#define MOJO_COMPILE_ASSERT(expr, msg) \
    typedef ::mojo::CompileAssert<(bool(expr))> msg[bool(expr) ? 1 : -1]
#else
#define MOJO_COMPILE_ASSERT(expr, msg)
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_MACROS_H_
