// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "components/update_client/protocol_builder.h"
#include "components/update_client/updater_state.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace update_client {

TEST(UpdateClientUtils, BuildProtocolRequest_ProdIdVersion) {
  // Verifies that |prod_id| and |version| are serialized.
  const string request = BuildProtocolRequest("some_prod_id", "1.0", "", "", "",
                                              "", "", "", nullptr);
  EXPECT_NE(string::npos, request.find(" version=\"some_prod_id-1.0\" "));
}

TEST(UpdateClientUtils, BuildProtocolRequest_DownloadPreference) {
  // Verifies that an empty |download_preference| is not serialized.
  const string request_no_dlpref =
      BuildProtocolRequest("", "", "", "", "", "", "", "", nullptr);
  EXPECT_EQ(string::npos, request_no_dlpref.find(" dlpref="));

  // Verifies that |download_preference| is serialized.
  const string request_with_dlpref =
      BuildProtocolRequest("", "", "", "", "", "some pref", "", "", nullptr);
  EXPECT_NE(string::npos, request_with_dlpref.find(" dlpref=\"some pref\""));
}

TEST(UpdateClientUtils, BuildProtocolRequestUpdaterStateAttributes) {
  // When no updater state is provided, then check that the elements and
  // attributes related to the updater state are not serialized.
  std::string request =
      BuildProtocolRequest("", "", "", "", "", "", "", "", nullptr).c_str();
  EXPECT_EQ(std::string::npos, request.find(" domainjoined"));
  EXPECT_EQ(std::string::npos, request.find("<updater"));

  UpdaterState::Attributes attributes;
  attributes["domainjoined"] = "1";
  attributes["name"] = "Omaha";
  attributes["version"] = "1.2.3.4";
  attributes["laststarted"] = "1";
  attributes["lastchecked"] = "2";
  attributes["autoupdatecheckenabled"] = "0";
  attributes["updatepolicy"] = "-1";
  request = BuildProtocolRequest(
      "", "", "", "", "", "", "", "",
      std::make_unique<UpdaterState::Attributes>(attributes));
  EXPECT_NE(std::string::npos, request.find(" domainjoined=\"1\""));
  const std::string updater_element =
      "<updater autoupdatecheckenabled=\"0\" "
      "lastchecked=\"2\" laststarted=\"1\" name=\"Omaha\" "
      "updatepolicy=\"-1\" version=\"1.2.3.4\"/>";
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_NE(std::string::npos, request.find(updater_element));
#else
  EXPECT_EQ(std::string::npos, request.find(updater_element));
#endif  // GOOGLE_CHROME_BUILD
}

}  // namespace update_client
