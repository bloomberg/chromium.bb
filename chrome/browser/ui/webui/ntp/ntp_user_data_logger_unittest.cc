// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"

#include "base/basictypes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockNTPUserDataLogger : public NTPUserDataLogger {
 public:
  MockNTPUserDataLogger() : NTPUserDataLogger(NULL) {}
  virtual ~MockNTPUserDataLogger() {}

  MOCK_CONST_METHOD2(GetPercentError, size_t(size_t errors, size_t events));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNTPUserDataLogger);
};

}  // namespace

TEST(NTPUserDataLoggerTest, ThumbnailErrorRateDoesNotDivideByZero) {
  MockNTPUserDataLogger logger;
  EXPECT_CALL(logger, GetPercentError(testing::_, testing::_)).Times(0);
  logger.EmitThumbnailErrorRate();
}
