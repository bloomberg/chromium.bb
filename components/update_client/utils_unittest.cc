// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "components/update_client/updater_state.h"
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

// Tests that the brand matches ^[a-zA-Z]{4}?$
TEST(UpdateClientUtils, IsValidBrand) {
  // The valid brand code must be empty or exactly 4 chars long.
  EXPECT_TRUE(IsValidBrand(std::string("")));
  EXPECT_TRUE(IsValidBrand(std::string("TEST")));
  EXPECT_TRUE(IsValidBrand(std::string("test")));
  EXPECT_TRUE(IsValidBrand(std::string("TEst")));

  EXPECT_FALSE(IsValidBrand(std::string("T")));      // Too short.
  EXPECT_FALSE(IsValidBrand(std::string("TE")));     //
  EXPECT_FALSE(IsValidBrand(std::string("TES")));    //
  EXPECT_FALSE(IsValidBrand(std::string("TESTS")));  // Too long.
  EXPECT_FALSE(IsValidBrand(std::string("TES1")));   // Has digit.
  EXPECT_FALSE(IsValidBrand(std::string(" TES")));   // Begins with white space.
  EXPECT_FALSE(IsValidBrand(std::string("TES ")));   // Ends with white space.
  EXPECT_FALSE(IsValidBrand(std::string("T ES")));   // Contains white space.
  EXPECT_FALSE(IsValidBrand(std::string("<TE")));    // Has <.
  EXPECT_FALSE(IsValidBrand(std::string("TE>")));    // Has >.
  EXPECT_FALSE(IsValidBrand(std::string("\"")));     // Has "
  EXPECT_FALSE(IsValidBrand(std::string("\\")));     // Has backslash.
  EXPECT_FALSE(IsValidBrand(std::string("\xaa")));   // Has non-ASCII char.
}

// Tests that the name of an InstallerAttribute matches ^[-_=a-zA-Z0-9]{1,256}$
TEST(UpdateClientUtils, IsValidInstallerAttributeName) {
  // Test the length boundaries.
  EXPECT_FALSE(IsValidInstallerAttribute(
      make_pair(std::string(0, 'a'), std::string("value"))));
  EXPECT_TRUE(IsValidInstallerAttribute(
      make_pair(std::string(1, 'a'), std::string("value"))));
  EXPECT_TRUE(IsValidInstallerAttribute(
      make_pair(std::string(256, 'a'), std::string("value"))));
  EXPECT_FALSE(IsValidInstallerAttribute(
      make_pair(std::string(257, 'a'), std::string("value"))));

  const char* const valid_names[] = {"A", "Z", "a", "a-b", "A_B",
                                     "z", "0", "9", "-_"};
  for (const char* name : valid_names)
    EXPECT_TRUE(IsValidInstallerAttribute(
        make_pair(std::string(name), std::string("value"))));

  const char* const invalid_names[] = {
      "",   "a=1", " name", "name ", "na me", "<name", "name>",
      "\"", "\\",  "\xaa",  ".",     ",",     ";",     "+"};
  for (const char* name : invalid_names)
    EXPECT_FALSE(IsValidInstallerAttribute(
        make_pair(std::string(name), std::string("value"))));
}

// Tests that the value of an InstallerAttribute matches
// ^[-.,;+_=a-zA-Z0-9]{0,256}$
TEST(UpdateClientUtils, IsValidInstallerAttributeValue) {
  // Test the length boundaries.
  EXPECT_TRUE(IsValidInstallerAttribute(
      make_pair(std::string("name"), std::string(0, 'a'))));
  EXPECT_TRUE(IsValidInstallerAttribute(
      make_pair(std::string("name"), std::string(256, 'a'))));
  EXPECT_FALSE(IsValidInstallerAttribute(
      make_pair(std::string("name"), std::string(257, 'a'))));

  const char* const valid_values[] = {"",  "a=1", "A", "Z",      "a",
                                      "z", "0",   "9", "-.,;+_="};
  for (const char* value : valid_values)
    EXPECT_TRUE(IsValidInstallerAttribute(
        make_pair(std::string("name"), std::string(value))));

  const char* const invalid_values[] = {" ap", "ap ", "a p", "<ap",
                                        "ap>", "\"",  "\\",  "\xaa"};
  for (const char* value : invalid_values)
    EXPECT_FALSE(IsValidInstallerAttribute(
        make_pair(std::string("name"), std::string(value))));
}

TEST(UpdateClientUtils, RemoveUnsecureUrls) {
  const GURL test1[] = {GURL("http://foo"), GURL("https://foo")};
  std::vector<GURL> urls(std::begin(test1), std::end(test1));
  RemoveUnsecureUrls(&urls);
  EXPECT_EQ(1u, urls.size());
  EXPECT_EQ(urls[0], GURL("https://foo"));

  const GURL test2[] = {GURL("https://foo"), GURL("http://foo")};
  urls.assign(std::begin(test2), std::end(test2));
  RemoveUnsecureUrls(&urls);
  EXPECT_EQ(1u, urls.size());
  EXPECT_EQ(urls[0], GURL("https://foo"));

  const GURL test3[] = {GURL("https://foo"), GURL("https://bar")};
  urls.assign(std::begin(test3), std::end(test3));
  RemoveUnsecureUrls(&urls);
  EXPECT_EQ(2u, urls.size());
  EXPECT_EQ(urls[0], GURL("https://foo"));
  EXPECT_EQ(urls[1], GURL("https://bar"));

  const GURL test4[] = {GURL("http://foo")};
  urls.assign(std::begin(test4), std::end(test4));
  RemoveUnsecureUrls(&urls);
  EXPECT_EQ(0u, urls.size());

  const GURL test5[] = {GURL("http://foo"), GURL("http://bar")};
  urls.assign(std::begin(test5), std::end(test5));
  RemoveUnsecureUrls(&urls);
  EXPECT_EQ(0u, urls.size());
}

}  // namespace update_client
