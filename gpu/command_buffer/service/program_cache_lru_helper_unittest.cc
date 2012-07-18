// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_cache_lru_helper.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class ProgramCacheLruHelperTest : public testing::Test {
 public:
  ProgramCacheLruHelperTest() :
    lru_helper_(new ProgramCacheLruHelper()) { }

 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
    lru_helper_->Clear();
  }

  scoped_ptr<ProgramCacheLruHelper> lru_helper_;
};

TEST_F(ProgramCacheLruHelperTest, ProgramCacheLruHelperEvictionOrderNoReuse) {
  lru_helper_->KeyUsed("1");
  lru_helper_->KeyUsed("2");
  lru_helper_->KeyUsed("3");
  lru_helper_->KeyUsed("4");
  const std::string* key = lru_helper_->PeekKey();
  EXPECT_EQ("1", *key);
  EXPECT_EQ("1", *lru_helper_->PeekKey());
  lru_helper_->PopKey();
  EXPECT_FALSE(lru_helper_->IsEmpty());
  EXPECT_EQ("2", *lru_helper_->PeekKey());
  lru_helper_->PopKey();
  EXPECT_FALSE(lru_helper_->IsEmpty());
  EXPECT_EQ("3", *lru_helper_->PeekKey());
  lru_helper_->PopKey();
  EXPECT_FALSE(lru_helper_->IsEmpty());
  EXPECT_EQ("4", *lru_helper_->PeekKey());
  lru_helper_->PopKey();
  EXPECT_TRUE(lru_helper_->IsEmpty());
}

TEST_F(ProgramCacheLruHelperTest, ProgramCacheLruHelperClear) {
  EXPECT_TRUE(lru_helper_->IsEmpty());
  lru_helper_->KeyUsed("1");
  lru_helper_->KeyUsed("2");
  lru_helper_->KeyUsed("3");
  lru_helper_->KeyUsed("4");
  EXPECT_FALSE(lru_helper_->IsEmpty());
  lru_helper_->Clear();
  EXPECT_TRUE(lru_helper_->IsEmpty());
}

TEST_F(ProgramCacheLruHelperTest, ProgramCacheLruHelperEvictionOrderWithReuse) {
  lru_helper_->KeyUsed("1");
  lru_helper_->KeyUsed("2");
  lru_helper_->KeyUsed("4");
  lru_helper_->KeyUsed("2");
  lru_helper_->KeyUsed("3");
  lru_helper_->KeyUsed("1");
  lru_helper_->KeyUsed("1");
  lru_helper_->KeyUsed("2");
  EXPECT_EQ("4", *lru_helper_->PeekKey());
  EXPECT_EQ("4", *lru_helper_->PeekKey());
  lru_helper_->PopKey();
  EXPECT_EQ("3", *lru_helper_->PeekKey());
  lru_helper_->PopKey();
  EXPECT_EQ("1", *lru_helper_->PeekKey());
  lru_helper_->PopKey();
  EXPECT_FALSE(lru_helper_->IsEmpty());
  EXPECT_EQ("2", *lru_helper_->PeekKey());
  lru_helper_->PopKey();
  EXPECT_TRUE(lru_helper_->IsEmpty());
}

}  // namespace gles2
}  // namespace gpu
