// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"

namespace {

class Parent {
};

class Child : public Parent {
};

class RefCountedClass : public base::RefCountedThreadSafe<RefCountedClass> {
};

}  // namespace

#if defined(NCTEST_NO_PASS_DOWNCAST)  // [r"fatal error: no viable conversion from returned value of type 'scoped_ptr<\(anonymous namespace\)::Parent, default_delete<\(anonymous namespace\)::Parent>>' to function return type 'scoped_ptr<\(anonymous namespace\)::Child, default_delete<\(anonymous namespace\)::Child>>'"]

scoped_ptr<Child> DowncastUsingPassAs(scoped_ptr<Parent> object) {
  return object.Pass();
}

#elif defined(NCTEST_NO_REF_COUNTED_SCOPED_PTR)  // [r"fatal error: static_assert failed \"T is a refcounted type and needs a scoped_refptr\""]

// scoped_ptr<> should not work for ref-counted objects.
void WontCompile() {
  scoped_ptr<RefCountedClass> x;
}

#elif defined(NCTEST_NO_ARRAY_WITH_SIZE)  // [r"fatal error: static_assert failed \"scoped_ptr doesn't support array with size\""]

void WontCompile() {
  scoped_ptr<int[10]> x;
}

#elif defined(NCTEST_NO_PASS_FROM_ARRAY)  // [r"fatal error: no viable overloaded '='"]

void WontCompile() {
  scoped_ptr<int[]> a;
  scoped_ptr<int*> b;
  b = a.Pass();
}

#elif defined(NCTEST_NO_PASS_TO_ARRAY)  // [r"fatal error: no viable overloaded '='"]

void WontCompile() {
  scoped_ptr<int*> a;
  scoped_ptr<int[]> b;
  b = a.Pass();
}

#elif defined(NCTEST_NO_CONSTRUCT_FROM_ARRAY)  // [r"fatal error: no matching constructor for initialization of 'scoped_ptr<int \*>'"]

void WontCompile() {
  scoped_ptr<int[]> a;
  scoped_ptr<int*> b(a.Pass());
}

#elif defined(NCTEST_NO_CONSTRUCT_TO_ARRAY)  // [r"fatal error: no matching constructor for initialization of 'scoped_ptr<int \[\]>'"]

void WontCompile() {
  scoped_ptr<int*> a;
  scoped_ptr<int[]> b(a.Pass());
}

#elif defined(NCTEST_NO_CONSTRUCT_SCOPED_PTR_ARRAY_FROM_NULL)  // [r"is ambiguous"]

void WontCompile() {
  scoped_ptr<int[]> x(NULL);
}

#elif defined(NCTEST_NO_CONSTRUCT_SCOPED_PTR_ARRAY_FROM_DERIVED)  // [r"fatal error: calling a private constructor of class 'scoped_ptr<\(anonymous namespace\)::Parent \[\], std::default_delete<\(anonymous namespace\)::Parent \[\]> >'"]

void WontCompile() {
  scoped_ptr<Parent[]> x(new Child[1]);
}

#elif defined(NCTEST_NO_RESET_SCOPED_PTR_ARRAY_FROM_NULL)  // [r"is ambiguous"]

void WontCompile() {
  scoped_ptr<int[]> x;
  x.reset(NULL);
}

#elif defined(NCTEST_NO_RESET_SCOPED_PTR_ARRAY_FROM_DERIVED)  // [r"fatal error: 'reset' is a private member of 'scoped_ptr<\(anonymous namespace\)::Parent \[\], std::default_delete<\(anonymous namespace\)::Parent \[\]> >'"]

void WontCompile() {
  scoped_ptr<Parent[]> x;
  x.reset(new Child[1]);
}

#elif defined(NCTEST_NO_DELETER_REFERENCE)  // [r"fatal error: base specifier must name a class"]

struct Deleter {
  void operator()(int*) {}
};

// Current implementation doesn't support Deleter Reference types. Enabling
// support would require changes to the behavior of the constructors to match
// including the use of SFINAE to discard the type-converting constructor
// as per C++11 20.7.1.2.1.19.
void WontCompile() {
  Deleter d;
  int n;
  scoped_ptr<int*, Deleter&> a(&n, d);
}

#endif
