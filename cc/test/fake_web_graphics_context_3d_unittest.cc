// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class ContextThatCountsMakeCurrents : public FakeWebGraphicsContext3D {
 public:
  ContextThatCountsMakeCurrents() : make_current_count_(0) {}

  // WebKit::WebGraphicsContext3D implementation.
  virtual bool makeContextCurrent() OVERRIDE {
    make_current_count_++;
    return true;
  }

  int make_current_count() const { return make_current_count_; }

 private:
  int make_current_count_;
};

TEST(FakeWebGraphicsContext3DTest, CreationShouldNotMakeCurrent) {
  scoped_ptr<ContextThatCountsMakeCurrents> context(
      new ContextThatCountsMakeCurrents);
  EXPECT_TRUE(context.get());
  EXPECT_EQ(0, context->make_current_count());
}

}  // namespace
}  // namespace cc
