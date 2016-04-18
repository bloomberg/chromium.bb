// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_exporter.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/export/csv_writer.h"

namespace password_manager {

namespace {

using autofill::PasswordForm;

// PasswordWriterBase ---------------------------------------------------------

// Interface for writing a list of passwords into various formats.
class PasswordWriterBase {
 public:
  virtual ~PasswordWriterBase() = 0;

  // Serializes the list of |passwords|.
  virtual std::string SerializePasswords(
      std::vector<scoped_ptr<PasswordForm>>& passwords) = 0;
};

PasswordWriterBase::~PasswordWriterBase() {}

// PasswordCSVWriter ----------------------------------------------------------

class PasswordCSVWriter : public PasswordWriterBase {
 public:
  static const base::FilePath::CharType kFileExtension[];

  PasswordCSVWriter() {}

  // PasswordWriterBase:
  std::string SerializePasswords(
      std::vector<scoped_ptr<PasswordForm>>& passwords) override {
    std::vector<std::string> header;
    header.push_back(kTitleFieldName);
    header.push_back(kUrlFieldName);
    header.push_back(kUsernameFieldName);
    header.push_back(kPasswordFieldName);

    std::vector<std::map<std::string, std::string>> records;
    records.reserve(passwords.size());
    for (const auto& it : passwords) {
      records.push_back(PasswordFormToRecord(it.get()));
    }
    std::string result;
    WriteCSV(header, records, &result);
    return result;
  }

 private:
  static const char kUrlFieldName[];
  static const char kUsernameFieldName[];
  static const char kPasswordFieldName[];
  static const char kTitleFieldName[];

  std::map<std::string, std::string> PasswordFormToRecord(
      const PasswordForm* form) {
    std::map<std::string, std::string> record;
    record[kUrlFieldName] = (form->origin).spec();
    record[kUsernameFieldName] = base::UTF16ToUTF8(form->username_value);
    record[kPasswordFieldName] = base::UTF16ToUTF8(form->password_value);
    record[kTitleFieldName] = form->origin.host();
    return record;
  }

  DISALLOW_COPY_AND_ASSIGN(PasswordCSVWriter);
};

const base::FilePath::CharType PasswordCSVWriter::kFileExtension[] =
    FILE_PATH_LITERAL("csv");
const char PasswordCSVWriter::kUrlFieldName[] = "url";
const char PasswordCSVWriter::kUsernameFieldName[] = "username";
const char PasswordCSVWriter::kPasswordFieldName[] = "password";
const char PasswordCSVWriter::kTitleFieldName[] = "name";

// Helper ---------------------------------------------------------------------

void WriteToFile(const base::FilePath& path, const std::string& data) {
  base::WriteFile(path, data.c_str(), data.size());
}

}  // namespace

// static
void PasswordExporter::Export(
    const base::FilePath& path,
    std::vector<scoped_ptr<PasswordForm>> passwords,
    scoped_refptr<base::TaskRunner> blocking_task_runner) {
  // Currently, CSV is the only supported format.
  PasswordCSVWriter password_writer;

  std::string serialized_passwords =
      password_writer.SerializePasswords(passwords);

  blocking_task_runner->PostTask(
      FROM_HERE, base::Bind(&WriteToFile, path, serialized_passwords));
}

// static
std::vector<std::vector<base::FilePath::StringType>>
PasswordExporter::GetSupportedFileExtensions() {
  std::vector<std::vector<base::FilePath::StringType>> extensions;
  extensions.resize(1);
  extensions[0].push_back(PasswordCSVWriter::kFileExtension);
  return extensions;
}

}  // namespace password_manager
