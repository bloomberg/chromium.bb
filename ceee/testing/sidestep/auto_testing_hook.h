// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility for using SideStep with unit tests.

#ifndef CEEE_TESTING_SIDESTEP_AUTO_TESTING_HOOK_H_
#define CEEE_TESTING_SIDESTEP_AUTO_TESTING_HOOK_H_

#include "base/basictypes.h"
#include "ceee/testing/sidestep/integration.h"
#include "ceee/testing/sidestep/preamble_patcher.h"

namespace sidestep {

// Same trick as common/scope_cleanup.h ScopeGuardImplBase
class AutoTestingHookBase {
 public:
  virtual ~AutoTestingHookBase() {}
};

// This is the typedef you normally use for the class, e.g.
//
// AutoTestingHook hook = MakeTestingHook(TargetFunc, HookTargetFunc);
//
// The 'hook' variable will then be destroyed when it goes out of scope.
//
// NOTE: You must not hold this type as a member of another class.  Its
// destructor will not get called.
typedef const AutoTestingHookBase& AutoTestingHook;

// This is the class you must use when holding a hook as a member of another
// class, e.g.
//
// public:
//  AutoTestingHookHolder holder_;
//  MyClass() : my_hook_holder(MakeTestingHookHolder(Target, Hook)) {}
class AutoTestingHookHolder {
 public:
  explicit AutoTestingHookHolder(AutoTestingHookBase* hook) : hook_(hook) {}
  ~AutoTestingHookHolder() { delete hook_; }
 private:
  AutoTestingHookHolder() {}  // disallow
  AutoTestingHookBase* hook_;
};

// This class helps patch a function, then unpatch it when the object exits
// scope, and also maintains the pointer to the original function stub.
//
// To enable use of the class without having to explicitly provide the
// type of the function pointers (and instead only providing it
// implicitly) we use the same trick as ScopeGuard (see
// common/scope_cleanup.h) uses, so to create a hook you use the MakeHook
// function rather than a constructor.
//
// NOTE:  This function is only safe for e.g. unit tests and _not_ for
// production code.  See PreamblePatcher class for details.
template <typename T>
class AutoTestingHookImpl : public AutoTestingHookBase {
 public:
  static AutoTestingHookImpl<T> MakeTestingHook(T target_function,
                                                T replacement_function,
                                                bool do_it) {
    return AutoTestingHookImpl<T>(target_function, replacement_function, do_it);
  }

  static AutoTestingHookImpl<T>* MakeTestingHookHolder(T target_function,
                                                       T replacement_function,
                                                       bool do_it) {
    return new AutoTestingHookImpl<T>(target_function,
                                      replacement_function, do_it);
  }

  ~AutoTestingHookImpl() {
    if (did_it_) {
      SIDESTEP_CHK(SIDESTEP_SUCCESS == PreamblePatcher::Unpatch(
                       target_function_, replacement_function_,
                       original_function_));
    }
  }

  // Returns a pointer to the original function.  To use this method you will
  // have to explicitly create an AutoTestingHookImpl of the specific
  // function pointer type (i.e. not use the AutoTestingHook typedef).
  T original_function() {
    return original_function_;
  }

 private:
  AutoTestingHookImpl(T target_function, T replacement_function, bool do_it)
      : target_function_(target_function),
        replacement_function_(replacement_function),
        original_function_(NULL),
        did_it_(do_it) {
    if (do_it) {
      SIDESTEP_CHK(SIDESTEP_SUCCESS == PreamblePatcher::Patch(target_function,
                                                     replacement_function,
                                                     &original_function_));
    }
  }

  T target_function_;  // always valid
  T original_function_;  // always valid
  T replacement_function_;  // always valid
  bool did_it_;  // Remember if we did it or not...
};

template <typename T>
inline AutoTestingHookImpl<T> MakeTestingHook(T target,
                                              T replacement,
                                              bool do_it) {
  return AutoTestingHookImpl<T>::MakeTestingHook(target, replacement, do_it);
}

template <typename T>
inline AutoTestingHookImpl<T> MakeTestingHook(T target, T replacement) {
  return AutoTestingHookImpl<T>::MakeTestingHook(target, replacement, true);
}

template <typename T>
inline AutoTestingHookImpl<T>* MakeTestingHookHolder(T target, T replacement) {
  return AutoTestingHookImpl<T>::MakeTestingHookHolder(target, replacement,
                                                       true);
}

};  // namespace sidestep

#endif  // CEEE_TESTING_SIDESTEP_AUTO_TESTING_HOOK_H_
