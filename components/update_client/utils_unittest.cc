// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using std::string;

namespace update_client {

TEST(BuildProtocolRequest, DownloadPreference) {
  const string emptystr;

  // Verifies that an empty |download_preference| is not serialized.
  const string request_no_dlpref = BuildProtocolRequest(
      emptystr, emptystr, emptystr, emptystr, emptystr, emptystr, emptystr);
  EXPECT_EQ(string::npos, request_no_dlpref.find(" dlpref="));

  // Verifies that |download_preference| is serialized.
  const string request_with_dlpref = BuildProtocolRequest(
      emptystr, emptystr, emptystr, emptystr, "some pref", emptystr, emptystr);
  EXPECT_NE(string::npos, request_with_dlpref.find(" dlpref=\"some pref\""));
}

}  // namespace update_client
