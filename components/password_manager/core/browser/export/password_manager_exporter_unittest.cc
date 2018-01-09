// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_manager_exporter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/export/password_csv_writer.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::ReturnArg;
using ::testing::StrEq;
using ::testing::StrictMock;

// A callback that matches the signature of base::WriteFile
using WriteCallback =
    base::RepeatingCallback<int(const base::FilePath&, const char*, int)>;

#if defined(OS_WIN)
const base::FilePath::CharType kNullFileName[] = FILE_PATH_LITERAL("/nul");
#else
const base::FilePath::CharType kNullFileName[] = FILE_PATH_LITERAL("/dev/null");
#endif

// Provides a predetermined set of credentials
class FakeCredentialProvider
    : public password_manager::CredentialProviderInterface {
 public:
  FakeCredentialProvider() {}

  void SetPasswordList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>&
          password_list) {
    password_list_.clear();
    for (const auto& form : password_list) {
      password_list_.push_back(std::make_unique<autofill::PasswordForm>(*form));
    }
  }

  // password_manager::CredentialProviderInterface:
  std::vector<std::unique_ptr<autofill::PasswordForm>> GetAllPasswords()
      override {
    std::vector<std::unique_ptr<autofill::PasswordForm>> ret_val;
    for (const auto& form : password_list_) {
      ret_val.push_back(std::make_unique<autofill::PasswordForm>(*form));
    }
    return ret_val;
  }

 private:
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list_;

  DISALLOW_COPY_AND_ASSIGN(FakeCredentialProvider);
};

// WriteFunction will delegate to this callback, if set. Use for setting
// expectations for base::WriteFile in PasswordManagerExporter.
base::MockCallback<WriteCallback>* g_write_callback = nullptr;

// Mock for base::WriteFile. Expectations should be set on |g_write_callback|.
int WriteFunction(const base::FilePath& filename, const char* data, int size) {
  if (g_write_callback)
    return g_write_callback->Get().Run(filename, data, size);
  return size;
}

// Creates a hardcoded set of credentials for tests.
std::vector<std::unique_ptr<autofill::PasswordForm>> CreatePasswordList() {
  auto password_form = std::make_unique<autofill::PasswordForm>();
  password_form->origin = GURL("http://accounts.google.com/a/LoginAuth");
  password_form->username_value = base::ASCIIToUTF16("test@gmail.com");
  password_form->password_value = base::ASCIIToUTF16("test1");

  std::vector<std::unique_ptr<autofill::PasswordForm>> password_forms;
  password_forms.push_back(std::move(password_form));
  return password_forms;
}

class PasswordManagerExporterTest : public testing::Test {
 public:
  PasswordManagerExporterTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        exporter_(&fake_credential_provider_),
        destination_path_(kNullFileName) {
    g_write_callback = &mock_write_file_;
    exporter_.SetWriteForTesting(&WriteFunction);
  }

  ~PasswordManagerExporterTest() override { g_write_callback = nullptr; }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  FakeCredentialProvider fake_credential_provider_;
  password_manager::PasswordManagerExporter exporter_;
  StrictMock<base::MockCallback<WriteCallback>> mock_write_file_;
  base::FilePath destination_path_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerExporterTest);
};

TEST_F(PasswordManagerExporterTest, PasswordExportSetPasswordListFirst) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list =
      CreatePasswordList();
  fake_credential_provider_.SetPasswordList(password_list);
  const std::string serialised(
      password_manager::PasswordCSVWriter::SerializePasswords(password_list));

  EXPECT_CALL(mock_write_file_,
              Run(destination_path_, StrEq(serialised), serialised.size()))
      .WillOnce(ReturnArg<2>());

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);

  scoped_task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerExporterTest, PasswordExportSetDestinationFirst) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list =
      CreatePasswordList();
  fake_credential_provider_.SetPasswordList(password_list);
  const std::string serialised(
      password_manager::PasswordCSVWriter::SerializePasswords(password_list));

  EXPECT_CALL(mock_write_file_,
              Run(destination_path_, StrEq(serialised), serialised.size()))
      .WillOnce(ReturnArg<2>());

  exporter_.SetDestination(destination_path_);
  exporter_.PreparePasswordsForExport();

  scoped_task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerExporterTest, DontExportWithOnlyDestination) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list =
      CreatePasswordList();
  fake_credential_provider_.SetPasswordList(password_list);

  EXPECT_CALL(mock_write_file_, Run(_, _, _)).Times(0);

  exporter_.SetDestination(destination_path_);

  scoped_task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerExporterTest, CancelAfterPasswords) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list =
      CreatePasswordList();
  fake_credential_provider_.SetPasswordList(password_list);

  EXPECT_CALL(mock_write_file_, Run(_, _, _)).Times(0);

  exporter_.PreparePasswordsForExport();
  exporter_.Cancel();
  exporter_.SetDestination(destination_path_);

  scoped_task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerExporterTest, CancelAfterDestination) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list =
      CreatePasswordList();
  fake_credential_provider_.SetPasswordList(password_list);

  EXPECT_CALL(mock_write_file_, Run(_, _, _)).Times(0);

  exporter_.SetDestination(destination_path_);
  exporter_.Cancel();
  exporter_.PreparePasswordsForExport();

  scoped_task_environment_.RunUntilIdle();
}

// Test that PasswordManagerExporter is reusable, after an export has been
// cancelled.
TEST_F(PasswordManagerExporterTest, CancelAfterPasswordsThenExport) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list =
      CreatePasswordList();
  const std::string serialised(
      password_manager::PasswordCSVWriter::SerializePasswords(password_list));
  fake_credential_provider_.SetPasswordList(password_list);

  EXPECT_CALL(mock_write_file_,
              Run(destination_path_, StrEq(serialised), serialised.size()))
      .WillOnce(ReturnArg<2>());

  exporter_.PreparePasswordsForExport();
  exporter_.Cancel();
  exporter_.SetDestination(destination_path_);
  exporter_.PreparePasswordsForExport();

  scoped_task_environment_.RunUntilIdle();
}

// Test that PasswordManagerExporter is reusable, after an export has been
// cancelled.
TEST_F(PasswordManagerExporterTest, CancelAfterDestinationThenExport) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list =
      CreatePasswordList();
  const std::string serialised(
      password_manager::PasswordCSVWriter::SerializePasswords(password_list));
  fake_credential_provider_.SetPasswordList(password_list);

  base::FilePath cancelled_path(FILE_PATH_LITERAL("clean_me_up"));

  EXPECT_CALL(mock_write_file_, Run(cancelled_path, _, _)).Times(0);
  EXPECT_CALL(mock_write_file_,
              Run(destination_path_, StrEq(serialised), serialised.size()))
      .WillOnce(ReturnArg<2>());

  exporter_.SetDestination(std::move(cancelled_path));
  exporter_.Cancel();
  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);

  scoped_task_environment_.RunUntilIdle();
}

}  // namespace
