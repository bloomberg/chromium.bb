// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/system/reboot/reboot_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

TEST(RebootUtil, SingleSetGetLastRebootType) {
  if (RebootUtil::SetLastRebootType(RebootShlib::RebootSource::FORCED)) {
    EXPECT_EQ(RebootUtil::GetLastRebootType(),
              RebootShlib::RebootSource::FORCED);
  }
}

TEST(RebootUtil, MultipleSetGetLastRebootType) {
  if (RebootUtil::SetLastRebootType(RebootShlib::RebootSource::FORCED)) {
    EXPECT_EQ(RebootUtil::GetLastRebootType(),
              RebootShlib::RebootSource::FORCED);
  }

  if (RebootUtil::SetLastRebootType(RebootShlib::RebootSource::OTA)) {
    EXPECT_EQ(RebootUtil::GetLastRebootType(), RebootShlib::RebootSource::OTA);
  }

  if (RebootUtil::SetLastRebootType(RebootShlib::RebootSource::FDR)) {
    EXPECT_EQ(RebootUtil::GetLastRebootType(), RebootShlib::RebootSource::FDR);
  }
}

}  // namespace chromecast
