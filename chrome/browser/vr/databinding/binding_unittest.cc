// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/databinding/binding.h"

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

namespace {

struct TestModel {
  bool value;
};

struct TestView {
  bool value;
  std::unique_ptr<Binding<bool>> binding;
};

}  // namespace

TEST(Binding, BoundBool) {
  TestModel a;
  a.value = true;

  TestView b;
  b.value = false;

  b.binding = VR_BIND_FIELD(bool, TestModel, &a, value, TestView, &b, value);

  EXPECT_NE(a.value, b.value);
  b.binding->Update();

  EXPECT_EQ(true, a.value);
  EXPECT_EQ(true, b.value);

  a.value = false;
  EXPECT_EQ(true, b.value);

  b.binding->Update();
  EXPECT_EQ(false, a.value);
  EXPECT_EQ(false, b.value);

  b.value = true;
  // Since this is a one way binding, Update will not detect a change in value
  // of b. Since a's value has not changed, a new value should not be pushed
  // to b.
  b.binding->Update();
  EXPECT_EQ(false, a.value);
  EXPECT_EQ(true, b.value);
}

TEST(Binding, Lifetime) {
  base::WeakPtr<BindingBase> binding;
  {
    TestModel a;
    TestView b;
    b.binding = VR_BIND_FIELD(bool, TestModel, &a, value, TestView, &b, value);
    binding = b.binding->GetWeakPtr();
  }
  // This test is not particularly useful, since we're just testing the behavior
  // of base::WeakPtr, but it confirms that when an object owning a binding
  // falls out of scope, weak pointers to its bindings are correctly
  // invalidated.
  EXPECT_FALSE(!!binding);
}

}  // namespace vr
