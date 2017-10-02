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

const char kTestCdmType[] = "Test CDM";
const char kTestCdmGuid[] = "62FE9C4B-384E-48FD-B28A-9F6F248BC8CC";
const char kTestPath[] = "/aa/bb";
const char kVersion1[] = "1.1.1.1";
const char kVersion2[] = "1.1.1.2";
const char kTestCodecs[] = "vp9,avc1";
const char kCodecDelimiter[] = ",";
const char kTestKeySystem[] = "com.example.somesystem";

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
                const std::string& supported_codecs,
                std::string supported_key_system,
                bool supports_sub_key_systems = false) {
    const std::vector<std::string> codecs =
        base::SplitString(supported_codecs, kCodecDelimiter,
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    cdm_registry_.RegisterCdm(
        CdmInfo(type, kTestCdmGuid, base::Version(version),
                base::FilePath::FromUTF8Unsafe(path), codecs,
                supported_key_system, supports_sub_key_systems));
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

 protected:
  CdmRegistryImpl cdm_registry_;
};

TEST_F(CdmRegistryImplTest, Register) {
  Register(kTestCdmType, kVersion1, kTestPath, kTestCodecs, kTestKeySystem,
           true);
  std::vector<CdmInfo> cdms = cdm_registry_.GetAllRegisteredCdms();
  ASSERT_EQ(1u, cdms.size());
  CdmInfo cdm = cdms[0];
  EXPECT_EQ(kTestCdmType, cdm.type);
  EXPECT_EQ(kVersion1, cdm.version.GetString());
  EXPECT_EQ(kTestPath, cdm.path.MaybeAsASCII());
  EXPECT_EQ(2u, cdm.supported_codecs.size());
  EXPECT_EQ("vp9", cdm.supported_codecs[0]);
  EXPECT_EQ("avc1", cdm.supported_codecs[1]);
  EXPECT_EQ(kTestKeySystem, cdm.supported_key_system);
  EXPECT_TRUE(cdm.supports_sub_key_systems);
}

TEST_F(CdmRegistryImplTest, ReRegister) {
  Register(kTestCdmType, kVersion1, "/bb/cc", "unknown", kTestKeySystem);
  EXPECT_TRUE(IsRegistered(kTestCdmType, kVersion1));

  // Now register same key system with different values.
  Register(kTestCdmType, kVersion1, kTestPath, kTestCodecs, kTestKeySystem);
  EXPECT_TRUE(IsRegistered(kTestCdmType, kVersion1));
}

TEST_F(CdmRegistryImplTest, MultipleVersions) {
  Register(kTestCdmType, kVersion1, kTestPath, kTestCodecs, kTestKeySystem);
  Register(kTestCdmType, kVersion2, "/bb/cc", "unknown", kTestKeySystem);
  EXPECT_TRUE(IsRegistered(kTestCdmType, kVersion1));
  EXPECT_TRUE(IsRegistered(kTestCdmType, kVersion2));
}

TEST_F(CdmRegistryImplTest, NewVersionInsertedFirst) {
  Register(kTestCdmType, kVersion1, kTestPath, kTestCodecs, kTestKeySystem);
  Register(kTestCdmType, kVersion2, "/bb/cc", "unknown", kTestKeySystem);

  const std::vector<std::string> versions = GetVersions(kTestCdmType);
  EXPECT_EQ(2u, versions.size());
  EXPECT_EQ(kVersion2, versions[0]);
  EXPECT_EQ(kVersion1, versions[1]);
}

}  // namespace content
