// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scopers help you manage ownership of a pointer, helping you easily manage a
// pointer within a scope, and automatically destroying the pointer at the end
// of a scope.  There are two main classes you will use, which correspond to the
// operators new/delete and new[]/delete[].
//
// Example usage (scoped_ptr<T>):
//   {
//     scoped_ptr<Foo> foo(new Foo("wee"));
//   }  // foo goes out of scope, releasing the pointer with it.
//
//   {
//     scoped_ptr<Foo> foo;          // No pointer managed.
//     foo.reset(new Foo("wee"));    // Now a pointer is managed.
//     foo.reset(new Foo("wee2"));   // Foo("wee") was destroyed.
//     foo.reset(new Foo("wee3"));   // Foo("wee2") was destroyed.
//     foo->Method();                // Foo::Method() called.
//     foo.get()->Method();          // Foo::Method() called.
//     SomeFunc(foo.release());      // SomeFunc takes ownership, foo no longer
//                                   // manages a pointer.
//     foo.reset(new Foo("wee4"));   // foo manages a pointer again.
//     foo.reset();                  // Foo("wee4") destroyed, foo no longer
//                                   // manages a pointer.
//   }  // foo wasn't managing a pointer, so nothing was destroyed.
//
// Example usage (scoped_ptr<T[]>):
//   {
//     scoped_ptr<Foo[]> foo(new Foo[100]);
//     foo.get()->Method();  // Foo::Method on the 0th element.
//     foo[10].Method();     // Foo::Method on the 10th element.
//   }
//
// Scopers are testable as booleans:
//   {
//     scoped_ptr<Foo> foo;
//     if (!foo)
//       foo.reset(new Foo());
//     if (foo)
//       LOG(INFO) << "This code is reached."
//   }
//
// These scopers also implement part of the functionality of C++11 unique_ptr
// in that they are "movable but not copyable."  You can use the scopers in
// the parameter and return types of functions to signify ownership transfer
// in to and out of a function.  When calling a function that has a scoper
// as the argument type, it must be called with an rvalue of a scoper, which
// can be created by using std::move(), or the result of another function that
// generates a temporary; passing by copy will NOT work.  Here is an example
// using scoped_ptr:
//
//   void TakesOwnership(scoped_ptr<Foo> arg) {
//     // Do something with arg.
//   }
//   scoped_ptr<Foo> CreateFoo() {
//     // No need for calling std::move() for returning a move-only value, or
//     // when you already have an rvalue as we do here.
//     return scoped_ptr<Foo>(new Foo("new"));
//   }
//   scoped_ptr<Foo> PassThru(scoped_ptr<Foo> arg) {
//     return arg;
//   }
//
//   {
//     scoped_ptr<Foo> ptr(new Foo("yay"));  // ptr manages Foo("yay").
//     TakesOwnership(std::move(ptr));       // ptr no longer owns Foo("yay").
//     scoped_ptr<Foo> ptr2 = CreateFoo();   // ptr2 owns the return Foo.
//     scoped_ptr<Foo> ptr3 =                // ptr3 now owns what was in ptr2.
//         PassThru(std::move(ptr2));        // ptr2 is correspondingly nullptr.
//   }
//
// Notice that if you do not call std::move() when returning from PassThru(), or
// when invoking TakesOwnership(), the code will not compile because scopers
// are not copyable; they only implement move semantics which require calling
// the std::move() function to signify a destructive transfer of state.
// CreateFoo() is different though because we are constructing a temporary on
// the return line and thus can avoid needing to call std::move().
//
// The conversion move-constructor properly handles upcast in initialization,
// i.e. you can use a scoped_ptr<Child> to initialize a scoped_ptr<Parent>:
//
//   scoped_ptr<Foo> foo(new Foo());
//   scoped_ptr<FooParent> parent(std::move(foo));

#ifndef BASE_MEMORY_SCOPED_PTR_H_
#define BASE_MEMORY_SCOPED_PTR_H_

// This is an implementation designed to match the anticipated future TR2
// implementation of the scoped_ptr class.

// TODO(dcheng): Clean up these headers, but there are likely lots of existing
// IWYU violations.
#include <stddef.h>
#include <stdlib.h>

#include <iosfwd>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/move.h"
#include "build/build_config.h"

namespace base {

// Function object which invokes 'free' on its parameter, which must be
// a pointer. Can be used to store malloc-allocated pointers in scoped_ptr:
//
// scoped_ptr<int, base::FreeDeleter> foo_ptr(
//     static_cast<int*>(malloc(sizeof(int))));
struct FreeDeleter {
  inline void operator()(void* ptr) const {
    free(ptr);
  }
};

}  // namespace base

template <typename T, typename D = std::default_delete<T>>
using scoped_ptr = std::unique_ptr<T, D>;

// A function to convert T* into scoped_ptr<T>
// Doing e.g. make_scoped_ptr(new FooBarBaz<type>(arg)) is a shorter notation
// for scoped_ptr<FooBarBaz<type> >(new FooBarBaz<type>(arg))
template <typename T>
scoped_ptr<T> make_scoped_ptr(T* ptr) {
  return scoped_ptr<T>(ptr);
}

#endif  // BASE_MEMORY_SCOPED_PTR_H_
