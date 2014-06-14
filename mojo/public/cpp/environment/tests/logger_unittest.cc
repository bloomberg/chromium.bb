// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/environment/default_logger.h"
#include "mojo/public/cpp/environment/environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

TEST(LoggerTest, Basic) {
  Environment environment;
  const MojoLogger* const logger = GetDefaultLogger();

  logger->LogMessage(MOJO_LOG_LEVEL_VERBOSE-1, "Logged at VERBOSE-1 level");
  logger->LogMessage(MOJO_LOG_LEVEL_VERBOSE, "Logged at VERBOSE level");
  logger->LogMessage(MOJO_LOG_LEVEL_INFO, "Logged at INFO level");
  logger->LogMessage(MOJO_LOG_LEVEL_WARNING, "Logged at WARNING level");
  logger->LogMessage(MOJO_LOG_LEVEL_ERROR, "Logged at ERROR level");

  // This should kill us:
  EXPECT_DEATH({
    logger->LogMessage(MOJO_LOG_LEVEL_FATAL, "Logged at FATAL level");
  }, "");
}

}  // namespace
}  // namespace mojo
