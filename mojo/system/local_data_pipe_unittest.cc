// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/local_data_pipe.h"

#include "base/memory/ref_counted.h"
#include "mojo/system/data_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

// Validate options.
TEST(LocalDataPipeTest, Creation) {
  // Get default options.
  MojoCreateDataPipeOptions default_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateOptions(NULL, &default_options));

  // Create using default options.
  {
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(default_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }

  // TODO(vtl): More.
}

// TODO(vtl): More.

}  // namespace
}  // namespace system
}  // namespace mojo
