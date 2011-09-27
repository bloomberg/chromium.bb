// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MAC_CLOSURE_BLOCKS_LEOPARD_COMPAT_H_
#define CONTENT_BROWSER_MAC_CLOSURE_BLOCKS_LEOPARD_COMPAT_H_
#pragma once

// libclosure (blocks) compatibility for Mac OS X 10.5 (Leopard)
//
// Background material:
// http://developer.apple.com/library/mac/#documentation/Cocoa/Conceptual/Blocks
// http://opensource.apple.com/source/libclosure/libclosure-38/
//
// Leopard doesn't support blocks. Chrome supports Leopard. Chrome needs to use
// blocks.
//
// In any file where you use blocks (any time you type ^{...}), you must
// #include this file to ensure that the runtime symbols referenced by code
// emitted by the compiler are marked for weak-import. This means that if
// these symbols are not present at runtime, the program will still load, but
// their values will be NULL.
//
// In any target (in the GYP sense) where you use blocks, you must also depend
// on the closure_blocks_leopard_compat target to ensure that these symbols
// will be available at link time, even when the 10.5 SDK is in use. This
// allows the continued use of the 10.5 SDK, which does not contain these
// symbols.
//
// This does not relieve you of the responsibility to not use blocks on
// Leopard. Because runtime support for Blocks still isn't present on that
// operating system, the weak-imported symbols will have value 0 and attempts
// to do anything meaningful with them will fail or crash. You must take care
// not to enter any codepath that uses blocks on Leopard. The base::mac::IsOS*
// family may be helpful.
//
// Although this scheme allows the use of the 10.5 SDK and 10.5 runtime in an
// application that uses blocks, it is still necessary to use a compiler that
// supports blocks. GCC 4.2 as shipped with Xcode 3.2 for Mac OS X 10.6
// qualifies, as do sufficiently recent versions of clang. GCC 4.2 as shipped
// with Xcode 3.1 for Mac OS X 10.5 does not qualify.

// _NSConcreteGlobalBlock and _NSConcreteStackBlock are private implementation
// details of libclosure defined in libclosure/libclosure-38/Block_private.h,
// but they're exposed from libSystem as public symbols, and the block-enabled
// compiler will emit code that references these symbols. Because the symbols
// aren't present in 10.5's libSystem, they must be declared as weak imports
// in any file that uses blocks. Any block-using file must #include this
// header to guarantee that the symbols will show up in linked output as weak
// imports when compiling for a 10.5 deployment target. Because the symbols
// are always present in 10.6 and higher, they do not need to be a weak
// imports when the deployment target is at least 10.6.
//
// Both GCC and clang emit references to these symbols, providing implicit
// declarations as needed, but respecting any user declaration when present.
// See gcc-5666.3/gcc/c-parser.c build_block_struct_initlist,
// gcc-5666.3/gcc/cp/parser.c build_block_struct_initlist, and
// clang-2.9/lib/CodeGen/CodeGenModule.cpp
// CodeGenModule::getNSConcreteGlobalBlock() and
// CodeGenModule::getNSConcreteStackBlock().

#include <AvailabilityMacros.h>

#if defined(MAC_OS_X_VERSION_10_6) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6  // SDK >= 10.6
// Get the system's own declarations of these things if using an SDK where
// they are present.
#include <Block.h>
#endif  // SDK >= 10.6

extern "C" {

#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_5  // DT <= 10.5
#define MAYBE_WEAK_IMPORT __attribute__((weak_import))
#else  // DT > 10.5
#define MAYBE_WEAK_IMPORT
#endif  // DT <= 10.5

MAYBE_WEAK_IMPORT extern void* _Block_copy(const void*);
MAYBE_WEAK_IMPORT extern void _Block_release(const void*);
MAYBE_WEAK_IMPORT extern void _Block_object_assign(void*,
                                                   const void*,
                                                   const int);
MAYBE_WEAK_IMPORT extern void _Block_object_dispose(const void*, const int);

MAYBE_WEAK_IMPORT extern void* _NSConcreteGlobalBlock[32];
MAYBE_WEAK_IMPORT extern void* _NSConcreteStackBlock[32];

#undef MAYBE_WEAK_IMPORT

}  // extern "C"

// Macros from <Block.h>, in case <Block.h> is not present.

#ifndef Block_copy
#define Block_copy(...) \
    ((__typeof(__VA_ARGS__))_Block_copy((const void *)(__VA_ARGS__)))
#endif

#ifndef Block_release
#define Block_release(...) _Block_release((const void *)(__VA_ARGS__))
#endif

#endif  // CONTENT_BROWSER_MAC_CLOSURE_BLOCKS_LEOPARD_COMPAT_H_
