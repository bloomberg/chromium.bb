// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_errors.h"

#include "base/files/file.h"
#include "chromeos/dbus/smbprovider/directory_entry.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace smb_client {

class SmbErrorsTest : public ::testing::Test {
 public:
  SmbErrorsTest() = default;
  ~SmbErrorsTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SmbErrorsTest);
};

TEST_F(SmbErrorsTest, SmbErrorToFileError) {
  EXPECT_EQ(base::File::FILE_OK, TranslateToFileError(smbprovider::ERROR_OK));
  EXPECT_EQ(base::File::FILE_ERROR_FAILED,
            TranslateToFileError(smbprovider::ERROR_FAILED));
  EXPECT_EQ(base::File::FILE_ERROR_IN_USE,
            TranslateToFileError(smbprovider::ERROR_IN_USE));
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS,
            TranslateToFileError(smbprovider::ERROR_EXISTS));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            TranslateToFileError(smbprovider::ERROR_NOT_FOUND));
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED,
            TranslateToFileError(smbprovider::ERROR_ACCESS_DENIED));
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED,
            TranslateToFileError(smbprovider::ERROR_TOO_MANY_OPENED));
  EXPECT_EQ(base::File::FILE_ERROR_NO_MEMORY,
            TranslateToFileError(smbprovider::ERROR_NO_MEMORY));
  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE,
            TranslateToFileError(smbprovider::ERROR_NO_SPACE));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_DIRECTORY,
            TranslateToFileError(smbprovider::ERROR_NOT_A_DIRECTORY));
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION,
            TranslateToFileError(smbprovider::ERROR_INVALID_OPERATION));
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY,
            TranslateToFileError(smbprovider::ERROR_SECURITY));
  EXPECT_EQ(base::File::FILE_ERROR_ABORT,
            TranslateToFileError(smbprovider::ERROR_ABORT));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_FILE,
            TranslateToFileError(smbprovider::ERROR_NOT_A_FILE));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_EMPTY,
            TranslateToFileError(smbprovider::ERROR_NOT_EMPTY));
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_URL,
            TranslateToFileError(smbprovider::ERROR_INVALID_URL));
  EXPECT_EQ(base::File::FILE_ERROR_IO,
            TranslateToFileError(smbprovider::ERROR_IO));

  EXPECT_EQ(base::File::FILE_ERROR_FAILED,
            TranslateToFileError(smbprovider::ERROR_DBUS_PARSE_FAILED));
}

}  // namespace smb_client
}  // namespace chromeos
