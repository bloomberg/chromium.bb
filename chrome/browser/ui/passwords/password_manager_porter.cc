// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_manager_porter.h"

#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/export/password_exporter.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
// The following are not used on Android due to the |SelectFileDialog| being
// unused.
#if !defined(OS_ANDROID)
// The default name of the password file when exporting.
constexpr base::FilePath::CharType kDefaultFileName[] =
    FILE_PATH_LITERAL("chrome_passwords");

// The default directory and filename when importing and exporting passwords.
base::FilePath GetDefaultFilepathForPasswordFile(
    const base::FilePath::StringType& default_extension) {
  base::FilePath default_path;
  PathService::Get(chrome::DIR_USER_DOCUMENTS, &default_path);
  return default_path.Append(kDefaultFileName).AddExtension(default_extension);
}
#endif

// A helper class for reading the passwords that have been imported.
class PasswordImportConsumer {
 public:
  explicit PasswordImportConsumer(Profile* profile);

  void ConsumePassword(password_manager::PasswordImporter::Result result,
                       const std::vector<autofill::PasswordForm>& forms);

 private:
  Profile* profile_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PasswordImportConsumer);
};

PasswordImportConsumer::PasswordImportConsumer(Profile* profile)
    : profile_(profile) {}

void PasswordImportConsumer::ConsumePassword(
    password_manager::PasswordImporter::Result result,
    const std::vector<autofill::PasswordForm>& forms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ImportPasswordFromCSVResult", result,
      password_manager::PasswordImporter::NUM_IMPORT_RESULTS);

  if (result != password_manager::PasswordImporter::SUCCESS)
    return;

  UMA_HISTOGRAM_COUNTS("PasswordManager.ImportedPasswordsPerUserInCSV",
                       forms.size());
  scoped_refptr<password_manager::PasswordStore> store(
      PasswordStoreFactory::GetForProfile(profile_,
                                          ServiceAccessType::EXPLICIT_ACCESS));
  if (store) {
    for (const autofill::PasswordForm& form : forms)
      store->AddLogin(form);
  }
}
}  // namespace

PasswordManagerPorter::PasswordManagerPorter(
    password_manager::CredentialProviderInterface*
        credential_provider_interface)
    : credential_provider_interface_(credential_provider_interface) {}

PasswordManagerPorter::~PasswordManagerPorter() {}

void PasswordManagerPorter::Store() {
  // In unittests a null WebContents means: "Abort creating the file Selector."
  if (!web_contents_)
    return;
  PresentFileSelector(web_contents_,
                      PasswordManagerPorter::Type::PASSWORD_EXPORT);
}

void PasswordManagerPorter::Load() {
  DCHECK(web_contents_);
  PresentFileSelector(web_contents_,
                      PasswordManagerPorter::Type::PASSWORD_IMPORT);
}

void PasswordManagerPorter::PresentFileSelector(
    content::WebContents* web_contents,
    Type type) {
// This method should never be called on Android (as there is no file selector),
// and the relevant IDS constants are not present for Android.
#if !defined(OS_ANDROID)
  DCHECK(web_contents);
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // Get the default file extension for password files.
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions =
      password_manager::PasswordExporter::GetSupportedFileExtensions();
  DCHECK(!file_type_info.extensions.empty());
  DCHECK(!file_type_info.extensions[0].empty());
  file_type_info.include_all_files = true;

  // Present the file selector dialogue.
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  ui::SelectFileDialog::Type file_selector_mode =
      ui::SelectFileDialog::SELECT_NONE;
  unsigned title = 0;
  switch (type) {
    case PASSWORD_IMPORT:
      file_selector_mode = ui::SelectFileDialog::SELECT_OPEN_FILE;
      title = IDS_PASSWORD_MANAGER_IMPORT_DIALOG_TITLE;
      break;
    case PASSWORD_EXPORT:
      file_selector_mode = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
      title = IDS_PASSWORD_MANAGER_EXPORT_DIALOG_TITLE;
      break;
  }
  // Check that a valid action has been chosen.
  DCHECK(file_selector_mode);
  DCHECK(title);

  select_file_dialog_->SelectFile(
      file_selector_mode, l10n_util::GetStringUTF16(title),
      GetDefaultFilepathForPasswordFile(file_type_info.extensions[0][0]),
      &file_type_info, 1, file_type_info.extensions[0][0],
      web_contents->GetTopLevelNativeWindow(), reinterpret_cast<void*>(type));
#endif
}

void PasswordManagerPorter::FileSelected(const base::FilePath& path,
                                         int index,
                                         void* params) {
  switch (reinterpret_cast<uintptr_t>(params)) {
    case PASSWORD_IMPORT:
      ImportPasswordsFromPath(path);
      break;
    case PASSWORD_EXPORT:
      ExportPasswordsToPath(path);
      break;
  }
}

void PasswordManagerPorter::ImportPasswordsFromPath(
    const base::FilePath& path) {
  // Set up a |PasswordImportConsumer| to process each password entry.
  std::unique_ptr<PasswordImportConsumer> form_consumer(
      new PasswordImportConsumer(profile_));
  password_manager::PasswordImporter::Import(
      path, base::Bind(&PasswordImportConsumer::ConsumePassword,
                       std::move(form_consumer)));
}

void PasswordManagerPorter::ExportPasswordsToPath(const base::FilePath& path) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list =
      credential_provider_interface_->GetAllPasswords();
  UMA_HISTOGRAM_COUNTS("PasswordManager.ExportedPasswordsPerUserInCSV",
                       password_list.size());
  password_manager::PasswordExporter::Export(path, std::move(password_list));
}
