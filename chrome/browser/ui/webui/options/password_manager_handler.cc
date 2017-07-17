// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/password_manager_handler.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/export/password_exporter.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/experiments.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/origin_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace options {

namespace {
// The following constants should be synchronized with the constants in
// chrome/browser/resources/options/password_manager_list.js.
const char kUrlField[] = "url";
const char kShownOriginField[] = "shownOrigin";
const char kIsAndroidUriField[] = "isAndroidUri";
const char kIsClickable[] = "isClickable";
const char kIsSecureField[] = "isSecure";
const char kUsernameField[] = "username";
const char kPasswordField[] = "password";
const char kFederationField[] = "federation";

// Copies from |form| to |entry| the origin, shown origin, whether the origin is
// Android URI, and whether the origin is secure.
void CopyOriginInfoOfPasswordForm(const autofill::PasswordForm& form,
                                  base::DictionaryValue* entry) {
  std::string shown_origin;
  GURL link_url;
  std::tie(shown_origin, link_url) =
      password_manager::GetShownOriginAndLinkUrl(form);
  entry->SetString(kShownOriginField, shown_origin);
  DCHECK(link_url.is_valid());
  entry->SetString(kUrlField,
                   url_formatter::FormatUrl(
                       link_url, url_formatter::kFormatUrlOmitNothing,
                       net::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
  entry->SetBoolean(
      kIsAndroidUriField,
      password_manager::IsValidAndroidFacetURI(form.signon_realm));
  entry->SetBoolean(kIsClickable, true);
  entry->SetBoolean(kIsSecureField, content::IsOriginSecure(link_url));
}

}  // namespace

PasswordManagerHandler::PasswordManagerHandler() {
  password_manager_presenter_.reset(new PasswordManagerPresenter(this));
}

PasswordManagerHandler::PasswordManagerHandler(
    std::unique_ptr<PasswordManagerPresenter> presenter)
    : password_manager_presenter_(std::move(presenter)) {}

PasswordManagerHandler::~PasswordManagerHandler() {}

Profile* PasswordManagerHandler::GetProfile() {
  return Profile::FromWebUI(web_ui());
}

#if !defined(OS_ANDROID)
gfx::NativeWindow PasswordManagerHandler::GetNativeWindow() const {
  return web_ui()->GetWebContents()->GetTopLevelNativeWindow();
}
#endif

void PasswordManagerHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static const OptionsStringResource resources[] = {
      {"androidUriSuffix", IDS_PASSWORDS_ANDROID_URI_SUFFIX},
      {"autoSigninTitle", IDS_PASSWORDS_AUTO_SIGNIN_TITLE},
      {"autoSigninDescription", IDS_PASSWORDS_AUTO_SIGNIN_DESCRIPTION},
      {"savedPasswordsTitle", IDS_PASSWORD_MANAGER_SHOW_PASSWORDS_TAB_TITLE},
      {"passwordExceptionsTitle", IDS_PASSWORD_MANAGER_EXCEPTIONS_TAB_TITLE},
      {"passwordSearchPlaceholder", IDS_PASSWORDS_PAGE_SEARCH_PASSWORDS},
      {"passwordShowButton", IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON},
      {"passwordHideButton", IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON},
      {"passwordsNoPasswordsDescription",
       IDS_PASSWORDS_PAGE_VIEW_NO_PASSWORDS_DESCRIPTION},
      {"passwordsNoExceptionsDescription",
       IDS_PASSWORDS_PAGE_VIEW_NO_EXCEPTIONS_DESCRIPTION},
      {"passwordManagerImportPasswordButtonText",
       IDS_PASSWORD_MANAGER_IMPORT_BUTTON},
      {"passwordManagerExportPasswordButtonText",
       IDS_PASSWORD_MANAGER_EXPORT_BUTTON},
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));

  RegisterTitle(localized_strings, "passwordsPage",
                IDS_PASSWORDS_EXCEPTIONS_WINDOW_TITLE);

  localized_strings->SetString("passwordManagerLearnMoreURL",
                               chrome::kPasswordManagerLearnMoreURL);
  localized_strings->SetString(
      "passwordsManagePasswordsLink",
      password_manager::kPasswordManagerAccountDashboardURL);

  std::string management_hostname =
      GURL(password_manager::kPasswordManagerAccountDashboardURL).host();
  base::string16 link_text = base::UTF8ToUTF16(management_hostname);
  size_t offset;
  base::string16 full_text = l10n_util::GetStringFUTF16(
      IDS_MANAGE_PASSWORDS_REMOTE_TEXT, link_text, &offset);

  localized_strings->SetString("passwordsManagePasswordsBeforeLinkText",
                               full_text.substr(0, offset));
  localized_strings->SetString("passwordsManagePasswordsLinkText",
                               full_text.substr(offset, link_text.size()));
  localized_strings->SetString("passwordsManagePasswordsAfterLinkText",
                               full_text.substr(offset + link_text.size()));

  bool disable_show_passwords = false;

#if defined(OS_WIN) && defined(USE_ASH)
  // We disable the ability to show passwords when running in Windows Metro
  // interface.  This is because we cannot pop native Win32 dialogs from the
  // Metro process.
  // TODO(wfh): Revisit this if Metro usage grows.
  if (chrome::IsNativeWindowInAsh(GetNativeWindow()))
    disable_show_passwords = true;
#endif

  localized_strings->SetBoolean("disableShowPasswords", disable_show_passwords);
}

void PasswordManagerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "updatePasswordLists",
      base::Bind(&PasswordManagerHandler::HandleUpdatePasswordLists,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeSavedPassword",
      base::Bind(&PasswordManagerHandler::HandleRemoveSavedPassword,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removePasswordException",
      base::Bind(&PasswordManagerHandler::HandleRemovePasswordException,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestShowPassword",
      base::Bind(&PasswordManagerHandler::HandleRequestShowPassword,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "importPassword",
      base::Bind(&PasswordManagerHandler::HandlePasswordImport,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "exportPassword",
      base::Bind(&PasswordManagerHandler::HandlePasswordExport,
                 base::Unretained(this)));
}

void PasswordManagerHandler::InitializeHandler() {
  password_manager_presenter_->Initialize();
}

void PasswordManagerHandler::InitializePage() {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordImportExport)) {
    web_ui()->CallJavascriptFunctionUnsafe(
        "PasswordManager.showImportExportButton");
  }
}

void PasswordManagerHandler::HandleRemoveSavedPassword(
    const base::ListValue* args) {
  std::string string_value = base::UTF16ToUTF8(ExtractStringValue(args));
  int index;
  if (base::StringToInt(string_value, &index) && index >= 0) {
    password_manager_presenter_->RemoveSavedPassword(
        static_cast<size_t>(index));
  }
}

void PasswordManagerHandler::HandleRemovePasswordException(
    const base::ListValue* args) {
  std::string string_value = base::UTF16ToUTF8(ExtractStringValue(args));
  int index;
  if (base::StringToInt(string_value, &index) && index >= 0) {
    password_manager_presenter_->RemovePasswordException(
        static_cast<size_t>(index));
  }
}

void PasswordManagerHandler::HandleRequestShowPassword(
    const base::ListValue* args) {
  int index;
  if (!ExtractIntegerValue(args, &index))
    NOTREACHED();

  password_manager_presenter_->RequestShowPassword(static_cast<size_t>(index));
}

void PasswordManagerHandler::ShowPassword(
    size_t index,
    const std::string& origin_url,
    const std::string& username,
    const base::string16& password_value) {
  // Call back the front end to reveal the password.
  web_ui()->CallJavascriptFunctionUnsafe("PasswordManager.showPassword",
                                         base::Value(static_cast<int>(index)),
                                         base::Value(password_value));
}

void PasswordManagerHandler::HandleUpdatePasswordLists(
    const base::ListValue* args) {
  password_manager_presenter_->UpdatePasswordLists();
}

void PasswordManagerHandler::SetPasswordList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list) {
  base::ListValue entries;
  base::string16 placeholder(base::ASCIIToUTF16("        "));
  for (const auto& saved_password : password_list) {
    std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
    CopyOriginInfoOfPasswordForm(*saved_password, entry.get());

    entry->SetString(kUsernameField, saved_password->username_value);
    // Use a placeholder value with the same length as the password.
    entry->SetString(
        kPasswordField,
        base::string16(saved_password->password_value.length(), ' '));
    if (!saved_password->federation_origin.unique()) {
      entry->SetString(
          kFederationField,
          l10n_util::GetStringFUTF16(
              IDS_PASSWORDS_VIA_FEDERATION,
              base::UTF8ToUTF16(saved_password->federation_origin.host())));
    }

    entries.Append(std::move(entry));
  }

  web_ui()->CallJavascriptFunctionUnsafe(
      "PasswordManager.setSavedPasswordsList", entries);
}

void PasswordManagerHandler::SetPasswordExceptionList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>&
        password_exception_list) {
  base::ListValue entries;
  for (const auto& exception : password_exception_list) {
    std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
    CopyOriginInfoOfPasswordForm(*exception,  entry.get());
    entries.Append(std::move(entry));
  }

  web_ui()->CallJavascriptFunctionUnsafe(
      "PasswordManager.setPasswordExceptionsList", entries);
}

void PasswordManagerHandler::FileSelected(const base::FilePath& path,
                                          int index,
                                          void* params) {
  switch (static_cast<FileSelectorCaller>(reinterpret_cast<intptr_t>(params))) {
    case IMPORT_FILE_SELECTED:
      ImportPasswordFileSelected(path);
      break;
    case EXPORT_FILE_SELECTED:
      ExportPasswordFileSelected(path);
      break;
  }
}

void PasswordManagerHandler::HandlePasswordImport(const base::ListValue* args) {
#if !defined(OS_ANDROID)  // This is never called on Android.
  ui::SelectFileDialog::FileTypeInfo file_type_info;

  file_type_info.extensions =
      password_manager::PasswordImporter::GetSupportedFileExtensions();
  DCHECK(!file_type_info.extensions.empty() &&
         !file_type_info.extensions[0].empty());
  file_type_info.include_all_files = true;
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_IMPORT_DIALOG_TITLE),
      base::FilePath(), &file_type_info, 1, file_type_info.extensions[0][0],
      web_ui()->GetWebContents()->GetTopLevelNativeWindow(),
      reinterpret_cast<void*>(IMPORT_FILE_SELECTED));
#endif
}

void PasswordManagerHandler::ImportPasswordFileSelected(
    const base::FilePath& path) {
  scoped_refptr<ImportPasswordResultConsumer> form_consumer(
      new ImportPasswordResultConsumer(GetProfile()));

  password_manager::PasswordImporter::Import(
      path, base::Bind(&ImportPasswordResultConsumer::ConsumePassword,
                       form_consumer));
}

PasswordManagerHandler::ImportPasswordResultConsumer::
    ImportPasswordResultConsumer(Profile* profile)
    : profile_(profile) {}

void PasswordManagerHandler::ImportPasswordResultConsumer::ConsumePassword(
    password_manager::PasswordImporter::Result result,
    const std::vector<autofill::PasswordForm>& forms) {
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
    for (const autofill::PasswordForm& form : forms) {
      store->AddLogin(form);
    }
  }
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.StorePasswordImportedFromCSVResult",
                        static_cast<bool>(store));
}

void PasswordManagerHandler::HandlePasswordExport(const base::ListValue* args) {
#if !defined(OS_ANDROID)  // This is never called on Android.
  if (!password_manager_presenter_->IsUserAuthenticated())
    return;

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions =
      password_manager::PasswordExporter::GetSupportedFileExtensions();
  DCHECK(!file_type_info.extensions.empty() &&
         !file_type_info.extensions[0].empty());
  file_type_info.include_all_files = true;
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EXPORT_DIALOG_TITLE),
      base::FilePath(), &file_type_info, 1, file_type_info.extensions[0][0],
      GetNativeWindow(), reinterpret_cast<void*>(EXPORT_FILE_SELECTED));
#endif
}

void PasswordManagerHandler::ExportPasswordFileSelected(
    const base::FilePath& path) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list =
      password_manager_presenter_->GetAllPasswords();
  UMA_HISTOGRAM_COUNTS("PasswordManager.ExportedPasswordsPerUserInCSV",
                       password_list.size());
  password_manager::PasswordExporter::Export(path, password_list);
}

}  // namespace options
