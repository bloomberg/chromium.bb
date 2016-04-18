// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/password_importer.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/import/csv_reader.h"

namespace password_manager {

namespace {

using autofill::PasswordForm;

// PasswordReaderBase ---------------------------------------------------------

// Interface for writing a list of passwords into various formats.
class PasswordReaderBase {
 public:
  virtual ~PasswordReaderBase() = 0;

  // Deserializes a list of passwords from |input| into |passwords|.
  virtual PasswordImporter::Result DeserializePasswords(
      const std::string& input,
      std::vector<PasswordForm>* passwords) = 0;
};

PasswordReaderBase::~PasswordReaderBase() {}

// PasswordCSVReader ----------------------------------------------------------

class PasswordCSVReader : public PasswordReaderBase {
 public:
  static const base::FilePath::CharType kFileExtension[];

  PasswordCSVReader() {}

  // PasswordWriterBase:
  PasswordImporter::Result DeserializePasswords(
      const std::string& input,
      std::vector<PasswordForm>* passwords) override {
    std::vector<std::string> header;
    std::vector<std::map<std::string, std::string>> records;
    if (!ReadCSV(input, &header, &records))
      return PasswordImporter::SYNTAX_ERROR;

    if (!GetActualFieldName(header, GetURLFieldNames(), url_field_name_) ||
        !GetActualFieldName(header, GetUsernameFieldNames(),
                            username_field_name_) ||
        !GetActualFieldName(header, GetPasswordFieldNames(),
                            password_field_name_))
      return PasswordImporter::SEMANTIC_ERROR;

    passwords->clear();
    passwords->reserve(records.size());

    for (const auto& record : records) {
      PasswordForm form;
      if (RecordToPasswordForm(record, &form))
        passwords->push_back(form);
    }
    return PasswordImporter::SUCCESS;
  }

 private:
  std::string url_field_name_;
  std::string username_field_name_;
  std::string password_field_name_;

  const std::vector<std::string> GetURLFieldNames() {
    std::vector<std::string> url_names;
    url_names.push_back("url");
    url_names.push_back("website");
    url_names.push_back("origin");
    url_names.push_back("hostname");
    return url_names;
  }

  const std::vector<std::string> GetUsernameFieldNames() {
    std::vector<std::string> username_names;
    username_names.push_back("username");
    username_names.push_back("user");
    username_names.push_back("login");
    username_names.push_back("account");
    return username_names;
  }

  const std::vector<std::string> GetPasswordFieldNames() {
    std::vector<std::string> password_names;
    password_names.push_back("password");
    return password_names;
  }

  bool RecordToPasswordForm(const std::map<std::string, std::string>& record,
                            PasswordForm* form) {
    if (!record.count(url_field_name_) || !record.count(username_field_name_) ||
        !record.count(password_field_name_)) {
      return false;
    }
    form->origin = GURL(record.at(url_field_name_));
    form->signon_realm = form->origin.GetOrigin().spec();
    form->username_value = base::UTF8ToUTF16(record.at(username_field_name_));
    form->password_value = base::UTF8ToUTF16(record.at(password_field_name_));
    return true;
  }

  bool GetActualFieldName(const std::vector<std::string>& header,
                          const std::vector<std::string>& options,
                          std::string& field_name) {
    auto it = std::find_if(header.begin(), header.end(),
                           [&options](const std::string& str) {
                             return std::count(options.begin(), options.end(),
                                               base::ToLowerASCII(str));
                           });

    if (it == header.end()) {
      return false;
    }

    field_name = *it;
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(PasswordCSVReader);
};

const base::FilePath::CharType PasswordCSVReader::kFileExtension[] =
    FILE_PATH_LITERAL("csv");

// Helpers --------------------------------------------------------------------

// Reads and returns the contents of the file at |path| as a string, or returns
// a scoped point containing a NULL if there was an error.
scoped_ptr<std::string> ReadFileToString(const base::FilePath& path) {
  scoped_ptr<std::string> contents(new std::string);
  if (!base::ReadFileToString(path, contents.get()))
    return scoped_ptr<std::string>();
  return contents;
}

// Parses passwords from |input| using |password_reader| and synchronously calls
// |completion| with the results.
static void ParsePasswords(
    scoped_ptr<PasswordReaderBase> password_reader,
    const PasswordImporter::CompletionCallback& completion,
    scoped_ptr<std::string> input) {
  std::vector<PasswordForm> passwords;
  PasswordImporter::Result result = PasswordImporter::IO_ERROR;
  if (input)
    result = password_reader->DeserializePasswords(*input, &passwords);
  completion.Run(result, passwords);
}

}  // namespace

// static
void PasswordImporter::Import(
    const base::FilePath& path,
    scoped_refptr<base::TaskRunner> blocking_task_runner,
    const CompletionCallback& completion) {
  // Currently, CSV is the only supported format.
  scoped_ptr<PasswordReaderBase> password_reader(new PasswordCSVReader);
  base::PostTaskAndReplyWithResult(
      blocking_task_runner.get(), FROM_HERE,
      base::Bind(&ReadFileToString, path),
      base::Bind(&ParsePasswords, base::Passed(&password_reader), completion));
}

// static
std::vector<std::vector<base::FilePath::StringType>>
PasswordImporter::GetSupportedFileExtensions() {
  std::vector<std::vector<base::FilePath::StringType>> extensions;
  extensions.resize(1);
  extensions[0].push_back(PasswordCSVReader::kFileExtension);
  return extensions;
}

}  // namespace password_manager
