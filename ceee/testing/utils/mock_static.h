// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Framework for mocking sets of static functions using SideStep, so that
// no code change is needed in the static functions themselves.
//
// Note that tiny functions may get inlined by the compiler in debug builds,
// so this approach is not suitable for tiny functions you need to unit test in
// release mode.  Mock classes based on this framework are further limited to
// use from a single thread at a time, they are non-nestable, and only intended
// for testing code.
//
// To define a new static mock class, the structure is as follows:
//
// === cut here ===
// MOCK_STATIC_CLASS_BEGIN(MockClassName)
//   MOCK_STATIC_INIT_BEGIN(MockClassName)
//     MOCK_STATIC_INIT(IsWindow);
//     MOCK_STATIC_INIT2(SomeClass::SomeStatic, SomeStatic);
//     ... more MOCK_STATIC_INIT(2) macros ...
//   MOCK_STATIC_INIT_END()
//
//   MOCK_STATIC1(BOOL, CALLBACK, IsWindow, HWND);
//   MOCK_STATIC3(BOOL,, SomeStatic, SomeClass*, int, int);
//   ... more MOCK_STATICX macros ...
// MOCK_STATIC_CLASS_END(MockClassName)
// === cut here ===
//
// Note that each function being mocked appears twice, once in the
// initialization map (between MOCK_STATIC_INIT_BEGIN and _END) and again
// in the method map (the MOCK_STATICX macros).  For further details see
// documentation of the macros.

#ifndef CEEE_TESTING_UTILS_MOCK_STATIC_H_
#define CEEE_TESTING_UTILS_MOCK_STATIC_H_

#include "base/scoped_vector.h"
#include "ceee/testing/sidestep/auto_testing_hook.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace testing {

// A base class for mock objects that mock sets of static functions.  Used via
// MOCK_STATIC_XYZ macros below.
class MockStatic {
 public:
  virtual ~MockStatic() {}

 protected:
  ScopedVector<sidestep::AutoTestingHookBase> hooks_;
};

}  // namespace testing

// Begins definition of a static mock class named ClassName.
#define MOCK_STATIC_CLASS_BEGIN(ClassName) \
  class ClassName : public testing::MockStatic { \
     public: \
      virtual ~ClassName() { \
        current_ = NULL; \
      } \
      static ClassName* GetCurrent() { return current_; } \
     private: \
       static ClassName* current_;  // NOLINT

// Begins the initialization map of a static mock class named ClassName.
#define MOCK_STATIC_INIT_BEGIN(ClassName) \
    public: \
     ClassName() { \
       current_ = this;

// Add this to the initialization map of a static mock class to mock a
// global function named function_name.
#define MOCK_STATIC_INIT(function_name) \
    hooks_.push_back(sidestep::MakeTestingHookHolder(::##function_name, \
                     &function_name##Hook));

// Add this to the initialization map of a static mock class to mock a
// static function that is not global (e.g. in a namespace or a class function)
// @param function The function to mock
// @param method_name The name of the mock method you are defining for this
//     function using one of the MOCK_STATICX macros below.
#define MOCK_STATIC_INIT2(function, method_name) \
    hooks_.push_back(sidestep::MakeTestingHookHolder(function, \
                     &method_name##Hook));

// Ends the initialization map of a static mock class.
#define MOCK_STATIC_INIT_END() \
    }

// Each of the following MOCK_STATICX macros defines a mock method taking X
// arguments and a corresponding static function, used as a hook on the
// production function, that redirects calls to your mock method for the
// lifetime of the static mock object.
//
// @param return_type Return type of the function you are mocking.
// @param cconv The calling convention of the function you are mocking.
// @param name The name of the mock method. For global functions, this must
//     match the name of the global function. For non-global static functions,
//     it must match the method_name you provided in the MOCK_STATIC_INIT2
//     macro.
// @param t1 The type of the 1st parameter of the function.
// @param t2 The type of the 2nd parameter of the function.
// @param t3 The type of the 3rd parameter of the function.
// @param t4 The type of the 4th parameter of the function.
// @param t5 The type of the 5th parameter of the function.
// @param t6 The type of the 6th parameter of the function.
// @param t7 The type of the 7th parameter of the function.
#define MOCK_STATIC0(return_type, cconv, name) \
  MOCK_METHOD0(name, return_type()); \
  static return_type cconv name##Hook () { \
    return GetCurrent()->name(); \
  }  // NOLINT
#define MOCK_STATIC1(return_type, cconv, name, t1) \
  MOCK_METHOD1(name, return_type(t1)); \
  static return_type cconv name##Hook (t1 p1) { \
    return GetCurrent()->name(p1); \
  }  // NOLINT
#define MOCK_STATIC2(return_type, cconv, name, t1, t2) \
  MOCK_METHOD2(name, return_type(t1, t2)); \
  static return_type cconv name##Hook (t1 p1, t2 p2) { \
    return GetCurrent()->name(p1, p2); \
  }  // NOLINT
#define MOCK_STATIC3(return_type, cconv, name, t1, t2, t3) \
  MOCK_METHOD3(name, return_type(t1, t2, t3)); \
  static return_type cconv name##Hook (t1 p1, t2 p2, t3 p3) { \
    return GetCurrent()->name(p1, p2, p3); \
  }  // NOLINT
#define MOCK_STATIC4(return_type, cconv, name, t1, t2, t3, t4) \
  MOCK_METHOD4(name, return_type(t1, t2, t3, t4)); \
  static return_type cconv name##Hook (t1 p1, t2 p2, t3 p3, t4 p4) { \
    return GetCurrent()->name(p1, p2, p3, p4); \
  }  // NOLINT
#define MOCK_STATIC5(return_type, cconv, name, \
                          t1, t2, t3, t4, t5) \
  MOCK_METHOD5(name, return_type(t1, t2, t3, t4, t5)); \
  static return_type cconv name##Hook (t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) { \
    return GetCurrent()->name(p1, p2, p3, p4, p5); \
  }  // NOLINT
#define MOCK_STATIC6(return_type, cconv, name, \
                          t1, t2, t3, t4, t5, t6) \
  MOCK_METHOD6(name, return_type(t1, t2, t3, t4, t5, t6)); \
  static return_type cconv name##Hook ( \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) { \
    return GetCurrent()->name(p1, p2, p3, p4, p5, p6); \
  }  // NOLINT
#define MOCK_STATIC7(return_type, cconv, name, \
                          t1, t2, t3, t4, t5, t6, t7) \
  MOCK_METHOD7(name, return_type(t1, t2, t3, t4, t5, t6, t7)); \
  static return_type cconv name##Hook ( \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) { \
    return GetCurrent()->name(p1, p2, p3, p4, p5, p6, p7); \
  }  // NOLINT

// Ends definition of a static mock class named ClassName.
#define MOCK_STATIC_CLASS_END(ClassName) \
    }; \
    __declspec(selectany) ClassName* ClassName::current_ = NULL;

#endif  // CEEE_TESTING_UTILS_MOCK_STATIC_H_
