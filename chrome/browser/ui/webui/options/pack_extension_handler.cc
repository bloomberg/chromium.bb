// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/pack_extension_handler.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

PackExtensionHandler::PackExtensionHandler() {
}

PackExtensionHandler::~PackExtensionHandler() {
  if (pack_job_.get())
    pack_job_->ClearClient();
}

void PackExtensionHandler::Initialize() {
}

void PackExtensionHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  RegisterTitle(localized_strings, "clearBrowserDataOverlay",
                IDS_CLEAR_BROWSING_DATA_TITLE);

  localized_strings->SetString("packExtensionOverlay",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_DIALOG_TITLE));
  localized_strings->SetString("packExtensionHeading",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_DIALOG_HEADING));
  localized_strings->SetString("packExtensionCommit",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_BUTTON));
  localized_strings->SetString("packExtensionRootDir",
      l10n_util::GetStringUTF16(
          IDS_EXTENSION_PACK_DIALOG_ROOT_DIRECTORY_LABEL));
  localized_strings->SetString("packExtensionPrivateKey",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_DIALOG_PRIVATE_KEY_LABEL));
  localized_strings->SetString("packExtensionBrowseButton",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_DIALOG_BROWSE));
}

void PackExtensionHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  web_ui_->RegisterMessageCallback("pack",
      base::Bind(&PackExtensionHandler::HandlePackMessage,
                 base::Unretained(this)));
}

void PackExtensionHandler::OnPackSuccess(const FilePath& crx_file,
                                         const FilePath& pem_file) {
  ListValue results;
  web_ui_->CallJavascriptFunction("OptionsPage.closeOverlay", results);

  ShowAlert(UTF16ToUTF8(PackExtensionJob::StandardSuccessMessage(crx_file,
                                                                 pem_file)));
}

void PackExtensionHandler::OnPackFailure(const std::string& error) {
  ShowAlert(error);
}

void PackExtensionHandler::HandlePackMessage(const ListValue* args) {
  std::string extension_path;
  std::string private_key_path;
  CHECK_EQ(2U, args->GetSize());
  CHECK(args->GetString(0, &extension_path));
  CHECK(args->GetString(1, &private_key_path));

  FilePath root_directory =
      FilePath::FromWStringHack(UTF8ToWide(extension_path));
  FilePath key_file = FilePath::FromWStringHack(UTF8ToWide(private_key_path));

  if (root_directory.empty()) {
    if (extension_path.empty()) {
      ShowAlert(l10n_util::GetStringUTF8(
          IDS_EXTENSION_PACK_DIALOG_ERROR_ROOT_REQUIRED));
    } else {
      ShowAlert(l10n_util::GetStringUTF8(
          IDS_EXTENSION_PACK_DIALOG_ERROR_ROOT_INVALID));
    }

    return;
  }

  if (!private_key_path.empty() && key_file.empty()) {
    ShowAlert(l10n_util::GetStringUTF8(
        IDS_EXTENSION_PACK_DIALOG_ERROR_KEY_INVALID));
    return;
  }

  pack_job_ = new PackExtensionJob(this, root_directory, key_file);
  pack_job_->Start();
}

void PackExtensionHandler::ShowAlert(const std::string& message) {
  ListValue arguments;
  arguments.Append(Value::CreateStringValue(message));
  web_ui_->CallJavascriptFunction("alert", arguments);
}
