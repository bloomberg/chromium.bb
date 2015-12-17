// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/safe_browsing/protocol_manager_helper.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingProtocolManagerHelperTest : public testing::Test {
 protected:
  std::string key_param_;

  void SetUp() override {
    std::string key = google_apis::GetAPIKey();
    if (!key.empty()) {
      key_param_ = base::StringPrintf("&key=%s",
          net::EscapeQueryParamValue(key, true).c_str());
    }
  }
};

TEST_F(SafeBrowsingProtocolManagerHelperTest, TestComposePver4Url) {
  std::string result = SafeBrowsingProtocolManagerHelper::ComposePver4Url(
      "a", "b", "c", "d", "e");
  EXPECT_EQ("a/b/c?alt=proto&client_id=d&client_version=e" + key_param_,
            result);
}

}  // namespace safe_browsing
