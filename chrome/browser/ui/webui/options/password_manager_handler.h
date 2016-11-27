// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_PASSWORD_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_PASSWORD_MANAGER_HANDLER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "components/password_manager/core/browser/import/password_importer.h"
#include "components/prefs/pref_member.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace options {

// The WebUI based PasswordUIView. Displays passwords in the web ui.
class PasswordManagerHandler : public OptionsPageUIHandler,
                               public PasswordUIView,
                               public ui::SelectFileDialog::Listener {
 public:
  // Enumeration of different callers of SelectFile.
  enum FileSelectorCaller {
    IMPORT_FILE_SELECTED,
    EXPORT_FILE_SELECTED,
  };

  PasswordManagerHandler();
  ~PasswordManagerHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializeHandler() override;
  void InitializePage() override;
  void RegisterMessages() override;

  // ui::SelectFileDialog::Listener implementation.
  // |params| is of type FileSelectorCaller which indicates direction of IO.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;

  // PasswordUIView implementation.
  Profile* GetProfile() override;
  void ShowPassword(
      size_t index,
      const std::string& origin_url,
      const std::string& username,
      const base::string16& password_value) override;
  void SetPasswordList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list)
      override;
  void SetPasswordExceptionList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>&
          password_exception_list) override;
#if !defined(OS_ANDROID)
  gfx::NativeWindow GetNativeWindow() const override;
#endif

 protected:
  // This constructor is used for testing only.
  explicit PasswordManagerHandler(
      std::unique_ptr<PasswordManagerPresenter> presenter);

 private:
  // Clears and then populates the list of passwords and password exceptions.
  // Called when the JS PasswordManager object is initialized.
  void HandleUpdatePasswordLists(const base::ListValue* args);

  // Removes a saved password entry.
  // |value| the entry index to be removed.
  void HandleRemoveSavedPassword(const base::ListValue* args);

  // Removes a saved password exception.
  // |value| the entry index to be removed.
  void HandleRemovePasswordException(const base::ListValue* args);

  // Requests the plain text password for an entry to be revealed.
  // |index| The index of the entry.
  void HandleRequestShowPassword(const base::ListValue* args);

  // Import from CSV/JSON file. The steps are:
  //   1. user click import button -> HandlePasswordImport() ->
  //   start file selector
  //   2. user selects file -> ImportPasswordFileSeleted() -> read to memory
  //   3. read completes -> ImportPasswordFileRead() -> store to PasswordStore
  void HandlePasswordImport(const base::ListValue* args);
  void ImportPasswordFileSelected(const base::FilePath& path);
  void ImportPasswordFileRead(password_manager::PasswordImporter::Result result,
                              const std::vector<autofill::PasswordForm>& forms);

  // Export to CSV/JSON file. The steps are:
  //   1. user click export button -> HandlePasswordExport() ->
  //   check OS password if necessary -> start file selector
  //   2. user selects file -> ExportPasswordFileSeleted() ->
  //   write to memory buffer -> start write operation
  void HandlePasswordExport(const base::ListValue* args);
  void ExportPasswordFileSelected(const base::FilePath& path);

  // A short class to persist imported password forms to password store.
  class ImportPasswordResultConsumer
      : public base::RefCountedThreadSafe<ImportPasswordResultConsumer> {
   public:
    explicit ImportPasswordResultConsumer(Profile* profile);

    void ConsumePassword(password_manager::PasswordImporter::Result result,
                         const std::vector<autofill::PasswordForm>& forms);

   private:
    friend class base::RefCountedThreadSafe<ImportPasswordResultConsumer>;

    ~ImportPasswordResultConsumer() {}

    Profile* profile_;
  };

  // User pref for storing accept languages.
  std::string languages_;

  std::unique_ptr<PasswordManagerPresenter> password_manager_presenter_;

  // File picker to import/export file path.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_PASSWORD_MANAGER_HANDLER_H_
