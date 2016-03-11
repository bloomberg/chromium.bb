// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MACROS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MACROS_H_

// This file defines macros that are only used by generated bindings.

// The C++ standard requires that static const members have an out-of-class
// definition (in a single compilation unit), but MSVC chokes on this (when
// language extensions, which are required, are enabled). (You're only likely to
// notice the need for a definition if you take the address of the member or,
// more commonly, pass it to a function that takes it as a reference argument --
// probably an STL function.) This macro makes MSVC do the right thing. See
// http://msdn.microsoft.com/en-us/library/34h23df8(v=vs.100).aspx for more
// information. This workaround does not appear to be necessary after VS2015.
// Use like:
//
// In the .h file:
//   struct Foo {
//     static const int kBar = 5;
//   };
//
// In the .cc file:
//   MOJO_STATIC_CONST_MEMBER_DEFINITION const int Foo::kBar;
#if defined(_MSC_VER) && _MSC_VER < 1900
#define MOJO_STATIC_CONST_MEMBER_DEFINITION __declspec(selectany)
#else
#define MOJO_STATIC_CONST_MEMBER_DEFINITION
#endif

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MACROS_H_
