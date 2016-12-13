// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_registry_impl.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "content/public/common/cdm_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

const char kTestKeySystemType[] = "test";
const char kTestPath[] = "/aa/bb";
const char kVersion1[] = "1.1.1.1";
const char kVersion2[] = "1.1.1.2";
const char kTestCodecs[] = "vp9,mp4";
const char kCodecDelimiter[] = ",";

// For simplicity and to make failures easier to diagnose, this test uses
// std::string instead of base::FilePath and std::vector<std::string>.
class CdmRegistryImplTest : public testing::Test {
 public:
  CdmRegistryImplTest() {}
  ~CdmRegistryImplTest() override {}

 protected:
  void Register(const std::string& type,
                const std::string& version,
                const std::string& path,
                const std::string& supported_codecs) {
    const std::vector<std::string> codecs =
        base::SplitString(supported_codecs, kCodecDelimiter,
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    cdm_registry_.RegisterCdm(CdmInfo(type, base::Version(version),
                                      base::FilePath::FromUTF8Unsafe(path),
                                      codecs));
  }

  bool IsRegistered(const std::string& type, const std::string& version) {
    for (const auto& cdm : cdm_registry_.GetAllRegisteredCdms()) {
      if (cdm.type == type && cdm.version.GetString() == version)
        return true;
    }
    return false;
  }

  std::vector<std::string> GetVersions(const std::string& type) {
    std::vector<std::string> versions;
    for (const auto& cdm : cdm_registry_.GetAllRegisteredCdms()) {
      if (cdm.type == type)
        versions.push_back(cdm.version.GetString());
    }
    return versions;
  }

 private:
  CdmRegistryImpl cdm_registry_;
};

// Note that KeySystemService is a singleton, and thus the actions of
// one test impact other tests. So each test defines a different key system
// name to avoid conflicts.

TEST_F(CdmRegistryImplTest, Register) {
  Register(kTestKeySystemType, kVersion1, kTestPath, kTestCodecs);
  EXPECT_TRUE(IsRegistered(kTestKeySystemType, kVersion1));
}

TEST_F(CdmRegistryImplTest, ReRegister) {
  Register(kTestKeySystemType, kVersion1, "/bb/cc", "unknown");
  EXPECT_TRUE(IsRegistered(kTestKeySystemType, kVersion1));

  // Now register same key system with different values.
  Register(kTestKeySystemType, kVersion1, kTestPath, kTestCodecs);
  EXPECT_TRUE(IsRegistered(kTestKeySystemType, kVersion1));
}

TEST_F(CdmRegistryImplTest, MultipleVersions) {
  Register(kTestKeySystemType, kVersion1, kTestPath, kTestCodecs);
  Register(kTestKeySystemType, kVersion2, "/bb/cc", "unknown");
  EXPECT_TRUE(IsRegistered(kTestKeySystemType, kVersion1));
  EXPECT_TRUE(IsRegistered(kTestKeySystemType, kVersion2));
}

TEST_F(CdmRegistryImplTest, NewVersionInsertedFirst) {
  Register(kTestKeySystemType, kVersion1, kTestPath, kTestCodecs);
  Register(kTestKeySystemType, kVersion2, "/bb/cc", "unknown");

  const std::vector<std::string> versions = GetVersions(kTestKeySystemType);
  EXPECT_EQ(2u, versions.size());
  EXPECT_EQ(kVersion2, versions[0]);
  EXPECT_EQ(kVersion1, versions[1]);
}

}  // namespace content
