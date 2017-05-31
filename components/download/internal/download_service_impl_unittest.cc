// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/download_service_impl.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {
namespace {

class DownloadServiceImplTest : public testing::Test {
 public:
  DownloadServiceImplTest() {}
  ~DownloadServiceImplTest() override = default;

  void SetUp() override {}

 protected:
 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadServiceImplTest);
};

}  // namespace

TEST_F(DownloadServiceImplTest, Dummy) {}

}  // namespace download
