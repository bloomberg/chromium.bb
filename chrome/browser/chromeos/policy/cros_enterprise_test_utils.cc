// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/cros_enterprise_test_utils.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/proto/chromeos/install_attributes.pb.h"
#include "chromeos/chromeos_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace test_utils {

void MarkAsEnterpriseOwned(const std::string& username,
                           const base::FilePath& temp_dir) {
  cryptohome::SerializedInstallAttributes install_attrs_proto;
  cryptohome::SerializedInstallAttributes::Attribute* attribute = NULL;

  attribute = install_attrs_proto.add_attributes();
  attribute->set_name(EnterpriseInstallAttributes::kAttrEnterpriseOwned);
  attribute->set_value("true");

  attribute = install_attrs_proto.add_attributes();
  attribute->set_name(EnterpriseInstallAttributes::kAttrEnterpriseUser);
  attribute->set_value(username);

  const base::FilePath install_attrs_file =
      temp_dir.AppendASCII("install_attributes.pb");
  const std::string install_attrs_blob(install_attrs_proto.SerializeAsString());
  ASSERT_EQ(static_cast<int>(install_attrs_blob.size()),
            file_util::WriteFile(install_attrs_file,
                                 install_attrs_blob.c_str(),
                                 install_attrs_blob.size()));
  ASSERT_TRUE(PathService::Override(chromeos::FILE_INSTALL_ATTRIBUTES,
                                    install_attrs_file));
}

}  // namespace test_utils

}  // namespace policy
