// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_info_util_win.h"

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Overrides |variables_name| to |value| for the lifetime of this class. Upon
// destruction, the previous value is restored.
class ScopedEnvironmentVariableOverride {
 public:
  ScopedEnvironmentVariableOverride(const std::string& variable_name,
                                    const std::string& value);
  ~ScopedEnvironmentVariableOverride();

 private:
  std::unique_ptr<base::Environment> environment_;
  std::string variable_name_;
  bool overridden_;
  bool was_set_;
  std::string old_value_;
};

ScopedEnvironmentVariableOverride::ScopedEnvironmentVariableOverride(
    const std::string& variable_name,
    const std::string& value)
    : environment_(base::Environment::Create()),
      variable_name_(variable_name),
      overridden_(false),
      was_set_(false) {
  was_set_ = environment_->GetVar(variable_name, &old_value_);
  overridden_ = environment_->SetVar(variable_name, value);
}

ScopedEnvironmentVariableOverride::~ScopedEnvironmentVariableOverride() {
  if (overridden_) {
    if (was_set_)
      environment_->SetVar(variable_name_, old_value_);
    else
      environment_->UnSetVar(variable_name_);
  }
}

}  // namespace

TEST(ModuleInfoUtilTest, GetCertificateInfoUnsigned) {
  base::FilePath path;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &path));
  CertificateInfo cert_info;
  GetCertificateInfo(path, &cert_info);
  EXPECT_EQ(CertificateType::NO_CERTIFICATE, cert_info.type);
  EXPECT_TRUE(cert_info.path.empty());
  EXPECT_TRUE(cert_info.subject.empty());
}

TEST(ModuleInfoUtilTest, GetCertificateInfoSigned) {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::string sysroot;
  ASSERT_TRUE(env->GetVar("SYSTEMROOT", &sysroot));

  base::FilePath path =
      base::FilePath::FromUTF8Unsafe(sysroot).Append(L"system32\\kernel32.dll");

  CertificateInfo cert_info;
  GetCertificateInfo(path, &cert_info);
  EXPECT_NE(CertificateType::NO_CERTIFICATE, cert_info.type);
  EXPECT_FALSE(cert_info.path.empty());
  EXPECT_FALSE(cert_info.subject.empty());
}

TEST(ModuleInfoUtilTest, GetEnvironmentVariablesMapping) {
  ScopedEnvironmentVariableOverride scoped_override("foo", "C:\\bar\\");

  // The mapping for these variables will be retrieved.
  std::vector<base::string16> environment_variables = {
      L"foo", L"SYSTEMROOT",
  };
  StringMapping string_mapping =
      GetEnvironmentVariablesMapping(environment_variables);

  ASSERT_EQ(2u, string_mapping.size());

  EXPECT_STREQ(L"c:\\bar", string_mapping[0].first.c_str());
  EXPECT_STREQ(L"%foo%", string_mapping[0].second.c_str());
  EXPECT_FALSE(string_mapping[1].second.empty());
}

const struct CollapsePathList {
  base::string16 expected_result;
  base::string16 test_case;
} kCollapsePathList[] = {
    // Negative testing (should not collapse this path).
    {L"c:\\a\\a.dll", L"c:\\a\\a.dll"},
    // These two are to test that we select the maximum collapsed path.
    {L"%foo%\\a.dll", L"c:\\foo\\a.dll"},
    {L"%x%\\a.dll", L"c:\\foo\\bar\\a.dll"},
    // Tests that only full path components are collapsed.
    {L"c:\\foo_bar\\a.dll", L"c:\\foo_bar\\a.dll"},
};

TEST(ModuleInfoUtilTest, CollapseMatchingPrefixInPath) {
  StringMapping string_mapping = {
      std::make_pair(L"c:\\foo", L"%foo%"),
      std::make_pair(L"c:\\foo\\bar", L"%x%"),
  };

  for (size_t i = 0; i < arraysize(kCollapsePathList); ++i) {
    base::string16 test_case = kCollapsePathList[i].test_case;
    CollapseMatchingPrefixInPath(string_mapping, &test_case);
    EXPECT_EQ(kCollapsePathList[i].expected_result, test_case);
  }
}
