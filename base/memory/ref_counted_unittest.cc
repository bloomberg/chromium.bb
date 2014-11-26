// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"

#include "base/test/opaque_ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SelfAssign : public base::RefCounted<SelfAssign> {
 protected:
  virtual ~SelfAssign() {}

 private:
  friend class base::RefCounted<SelfAssign>;
};

class Derived : public SelfAssign {
 protected:
  ~Derived() override {}

 private:
  friend class base::RefCounted<Derived>;
};

class CheckDerivedMemberAccess : public scoped_refptr<SelfAssign> {
 public:
  CheckDerivedMemberAccess() {
    // This shouldn't compile if we don't have access to the member variable.
    SelfAssign** pptr = &ptr_;
    EXPECT_EQ(*pptr, ptr_);
  }
};

class ScopedRefPtrToSelf : public base::RefCounted<ScopedRefPtrToSelf> {
 public:
  ScopedRefPtrToSelf() : self_ptr_(this) {}

  static bool was_destroyed() { return was_destroyed_; }

  void SelfDestruct() { self_ptr_ = NULL; }

 private:
  friend class base::RefCounted<ScopedRefPtrToSelf>;
  ~ScopedRefPtrToSelf() { was_destroyed_ = true; }

  static bool was_destroyed_;

  scoped_refptr<ScopedRefPtrToSelf> self_ptr_;
};

bool ScopedRefPtrToSelf::was_destroyed_ = false;

}  // end namespace

TEST(RefCountedUnitTest, TestSelfAssignment) {
  SelfAssign* p = new SelfAssign;
  scoped_refptr<SelfAssign> var(p);
  var = var;
  EXPECT_EQ(var.get(), p);
}

TEST(RefCountedUnitTest, ScopedRefPtrMemberAccess) {
  CheckDerivedMemberAccess check;
}

TEST(RefCountedUnitTest, ScopedRefPtrToSelf) {
  ScopedRefPtrToSelf* check = new ScopedRefPtrToSelf();
  EXPECT_FALSE(ScopedRefPtrToSelf::was_destroyed());
  check->SelfDestruct();
  EXPECT_TRUE(ScopedRefPtrToSelf::was_destroyed());
}

TEST(RefCountedUnitTest, ScopedRefPtrToOpaque) {
  scoped_refptr<base::OpaqueRefCounted> p = base::MakeOpaqueRefCounted();
  base::TestOpaqueRefCounted(p);

  scoped_refptr<base::OpaqueRefCounted> q;
  q = p;
  base::TestOpaqueRefCounted(p);
  base::TestOpaqueRefCounted(q);
}

TEST(RefCountedUnitTest, BooleanTesting) {
  scoped_refptr<SelfAssign> p;
  EXPECT_FALSE(p);
  p = new SelfAssign;
  EXPECT_TRUE(p);
}

TEST(RefCountedUnitTest, Equality) {
  scoped_refptr<SelfAssign> p1(new SelfAssign);
  scoped_refptr<SelfAssign> p2(new SelfAssign);

  EXPECT_EQ(p1, p1);
  EXPECT_EQ(p2, p2);

  EXPECT_NE(p1, p2);
  EXPECT_NE(p2, p1);
}

TEST(RefCountedUnitTest, ConvertibleEquality) {
  scoped_refptr<Derived> p1(new Derived);
  scoped_refptr<SelfAssign> p2;

  EXPECT_NE(p1, p2);
  EXPECT_NE(p2, p1);

  p2 = p1;

  EXPECT_EQ(p1, p2);
  EXPECT_EQ(p2, p1);
}
