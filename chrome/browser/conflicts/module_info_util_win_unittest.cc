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
