// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_installer/win/app_installer_util.h"

#include <map>
#include <string>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_installer {

TEST(AppInstallerUtilTest, ParseTag) {
  std::map<std::string, std::string> parsed_pairs;

  parsed_pairs.clear();
  EXPECT_TRUE(ParseTag("key1=value1&key2=value2", &parsed_pairs));
  EXPECT_EQ(2, parsed_pairs.size());
  EXPECT_EQ("value1", parsed_pairs["key1"]);
  EXPECT_EQ("value2", parsed_pairs["key2"]);

  parsed_pairs.clear();
  EXPECT_FALSE(ParseTag("a&b", &parsed_pairs));

  parsed_pairs.clear();
  EXPECT_FALSE(ParseTag("#=a", &parsed_pairs));

  parsed_pairs.clear();
  EXPECT_FALSE(ParseTag("a=\01", &parsed_pairs));
}

void TestFetchUrlWithScheme(net::SpawnedTestServer::Type type, bool success) {
  net::SpawnedTestServer http_server(
      type, net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(http_server.Start());

  std::vector<uint8_t> response_data;
  net::HostPortPair host_port = http_server.host_port_pair();
  EXPECT_EQ(success,
            FetchUrl(L"user agent", base::SysUTF8ToWide(host_port.host()),
                     host_port.port(), L"files/extensions/app/manifest.json",
                     &response_data));
  if (success) {
    EXPECT_TRUE(response_data.size());
    base::FilePath source_root;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &source_root));
    base::FilePath file_path = source_root.Append(
        FILE_PATH_LITERAL("chrome/test/data/extensions/app/manifest.json"));
    std::string file_contents;
    EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
    EXPECT_EQ(file_contents,
              std::string(response_data.begin(), response_data.end()));
  } else {
    EXPECT_FALSE(response_data.size());
  }

  ASSERT_TRUE(http_server.Stop());
}

TEST(AppInstallerUtilTest, FetchUrlHttps) {
  TestFetchUrlWithScheme(net::SpawnedTestServer::TYPE_HTTPS, true);
}

TEST(AppInstallerUtilTest, FetchUrlNoHttps) {
  TestFetchUrlWithScheme(net::SpawnedTestServer::TYPE_HTTP, false);
}

}  // namespace app_installer
