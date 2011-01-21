// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A mixin class that makes it easy to assert on test object
// instance counts.
#ifndef CEEE_TESTING_UTILS_INSTANCE_COUNT_MIXIN_H_
#define CEEE_TESTING_UTILS_INSTANCE_COUNT_MIXIN_H_

#include <string>
#include <set>

#include "base/synchronization/lock.h"

namespace testing {

// A base class for the InstanceCountMixin template class below.
// The base class keeps a set of all derived class instances,
// which allows tests to assert on object counts and to enumerate
// leaked objects.
class InstanceCountMixinBase {
 public:
  InstanceCountMixinBase();
  virtual ~InstanceCountMixinBase();

  // Override this in subclasses to provide a description of the instance.
  // @note If you find that your tests are leaking objects, it can make your
  //    life easier to override this function in your test classes and invoke
  //    LogLeakedObjects to get a handle on the type and identity of the leaked
  //    objects.
  virtual void GetDescription(std::string* description) const;

  // Logs the description of every instance to the error log.
  static void LogLeakedInstances();

  // @return the count of all InstanceCountMixinBase-derived instances.
  static size_t all_instance_count();

  typedef std::set<InstanceCountMixinBase*> InstanceSet;

  // @return the start enumerator over the existing instances.
  // @note use of this enumerator is not thread safe.
  static InstanceSet::const_iterator begin();

  // @return the end enumerator over the existing instances.
  // @note use of this enumerator is not thread safe.
  static InstanceSet::const_iterator end();

 protected:
  static base::Lock lock_;
};

// Inherit test classes from this class to get a per-class instance count.
// Usage:
// class TestFoo: public InstanceCountMixin<TestFoo> {
// }
//
// EXPECT_EQ(0, TestFoo::instance_count());
template <class T>
class InstanceCountMixin : public InstanceCountMixinBase {
 public:
  InstanceCountMixin() {
    base::AutoLock lock(lock_);
    ++instance_count_;
  }
  ~InstanceCountMixin() {
    base::AutoLock lock(lock_);
    --instance_count_;
  }

  // @return the number of T instances.
  static size_t instance_count() { return instance_count_; }

 private:
  static size_t instance_count_;
};

template <class T> size_t InstanceCountMixin<T>::instance_count_ = 0;

}  // namespace testing

#endif  // CEEE_TESTING_UTILS_INSTANCE_COUNT_MIXIN_H_
