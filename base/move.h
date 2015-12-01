// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MOVE_H_
#define BASE_MOVE_H_

#include <utility>

#include "base/compiler_specific.h"

// Macro with the boilerplate that makes a type move-only in C++11.
//
// USAGE
//
// This macro should be used instead of DISALLOW_COPY_AND_ASSIGN to create
// a "move-only" type.  Unlike DISALLOW_COPY_AND_ASSIGN, this macro should be
// the first line in a class declaration.
//
// A class using this macro must call .Pass() (or somehow be an r-value already)
// before it can be:
//
//   * Passed as a function argument
//   * Used as the right-hand side of an assignment
//   * Returned from a function
//
// Each class will still need to define their own move constructor and move
// operator= to make this useful.  Here's an example of the macro, the move
// constructor, and the move operator= from a hypothetical scoped_ptr class:
//
//  template <typename T>
//  class scoped_ptr {
//    MOVE_ONLY_TYPE_WITH_MOVE_CONSTRUCTOR_FOR_CPP_03(type);
//   public:
//    scoped_ptr(scoped_ptr&& other) : ptr_(other.release()) { }
//    scoped_ptr& operator=(scoped_ptr&& other) {
//      reset(other.release());
//      return *this;
//    }
//  };
//
//
// WHY HAVE typedef void MoveOnlyTypeForCPP03
//
// Callback<>/Bind() needs to understand movable-but-not-copyable semantics
// to call .Pass() appropriately when it is expected to transfer the value.
// The cryptic typedef MoveOnlyTypeForCPP03 is added to make this check
// easy and automatic in helper templates for Callback<>/Bind().
// See IsMoveOnlyType template and its usage in base/callback_internal.h
// for more details.

#define MOVE_ONLY_TYPE_FOR_CPP_03(type) \
  MOVE_ONLY_TYPE_WITH_MOVE_CONSTRUCTOR_FOR_CPP_03(type)

#define MOVE_ONLY_TYPE_WITH_MOVE_CONSTRUCTOR_FOR_CPP_03(type)   \
 private:                                                       \
  type(const type&) = delete;                                   \
  void operator=(const type&) = delete;                         \
                                                                \
 public:                                                        \
  type&& Pass() WARN_UNUSED_RESULT { return std::move(*this); } \
  typedef void MoveOnlyTypeForCPP03;                            \
                                                                \
 private:

#define TYPE_WITH_MOVE_CONSTRUCTOR_FOR_CPP_03(type) \
 public: \
  type&& Pass() WARN_UNUSED_RESULT { return std::move(*this); } \
 private:

#endif  // BASE_MOVE_H_
