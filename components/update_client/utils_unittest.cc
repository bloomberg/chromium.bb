// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "components/update_client/utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using std::string;

namespace {

base::FilePath MakeTestFilePath(const char* file) {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.AppendASCII("components/test/data/update_client")
      .AppendASCII(file);
}

}  // namespace

namespace update_client {

TEST(UpdateClientUtils, BuildProtocolRequest_DownloadPreference) {
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

TEST(UpdateClientUtils, VerifyFileHash256) {
  EXPECT_TRUE(VerifyFileHash256(
      MakeTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"),
      std::string(
          "6fc4b93fd11134de1300c2c0bb88c12b644a4ec0fd7c9b12cb7cc067667bde87")));

  EXPECT_FALSE(VerifyFileHash256(
      MakeTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"),
      std::string("")));

  EXPECT_FALSE(VerifyFileHash256(
      MakeTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"),
      std::string("abcd")));

  EXPECT_FALSE(VerifyFileHash256(
      MakeTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"),
      std::string(
          "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
}

}  // namespace update_client
