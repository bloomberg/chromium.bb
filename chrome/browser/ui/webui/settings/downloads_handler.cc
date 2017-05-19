// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/downloads_handler.h"

#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

namespace settings {

DownloadsHandler::DownloadsHandler(Profile* profile) : profile_(profile) {}

DownloadsHandler::~DownloadsHandler() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_folder_dialog_.get())
    select_folder_dialog_->ListenerDestroyed();
}

void DownloadsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initializeDownloads",
      base::Bind(&DownloadsHandler::HandleInitialize, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetAutoOpenFileTypes",
      base::Bind(&DownloadsHandler::HandleResetAutoOpenFileTypes,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "selectDownloadLocation",
      base::Bind(&DownloadsHandler::HandleSelectDownloadLocation,
                 base::Unretained(this)));
}

void DownloadsHandler::OnJavascriptAllowed() {
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      prefs::kDownloadExtensionsToOpen,
      base::Bind(&DownloadsHandler::SendAutoOpenDownloadsToJavascript,
                 base::Unretained(this)));
}

void DownloadsHandler::OnJavascriptDisallowed() {
  pref_registrar_.RemoveAll();
}

void DownloadsHandler::HandleInitialize(const base::ListValue* args) {
  AllowJavascript();
  SendAutoOpenDownloadsToJavascript();
}

void DownloadsHandler::SendAutoOpenDownloadsToJavascript() {
  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(profile_);
  bool auto_open_downloads =
      DownloadPrefs::FromDownloadManager(manager)->IsAutoOpenUsed();
  FireWebUIListener("auto-open-downloads-changed",
                    base::Value(auto_open_downloads));
}

void DownloadsHandler::HandleResetAutoOpenFileTypes(
    const base::ListValue* args) {
  base::RecordAction(UserMetricsAction("Options_ResetAutoOpenFiles"));
  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(profile_);
  DownloadPrefs::FromDownloadManager(manager)->ResetAutoOpen();
}

void DownloadsHandler::HandleSelectDownloadLocation(
    const base::ListValue* args) {
  PrefService* pref_service = profile_->GetPrefs();
  select_folder_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  ui::SelectFileDialog::FileTypeInfo info;
  info.allowed_paths = ui::SelectFileDialog::FileTypeInfo::NATIVE_OR_DRIVE_PATH;
  select_folder_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_SETTINGS_DOWNLOAD_LOCATION),
      pref_service->GetFilePath(prefs::kDownloadDefaultDirectory), &info, 0,
      base::FilePath::StringType(),
      web_ui()->GetWebContents()->GetTopLevelNativeWindow(), NULL);
}

void DownloadsHandler::FileSelected(const base::FilePath& path,
                                    int index,
                                    void* params) {
  base::RecordAction(UserMetricsAction("Options_SetDownloadDirectory"));
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetFilePath(prefs::kDownloadDefaultDirectory, path);
  pref_service->SetFilePath(prefs::kSaveFileDefaultDirectory, path);
}

}  // namespace settings
