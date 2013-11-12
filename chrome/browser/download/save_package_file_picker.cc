// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/save_package_file_picker.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/file_util_icu.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/download_handler.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#endif

using content::RenderProcessHost;
using content::SavePageType;
using content::WebContents;

namespace {

// If false, we don't prompt the user as to where to save the file.  This
// exists only for testing.
bool g_should_prompt_for_filename = true;

#if !defined(OS_CHROMEOS)
// Used for mapping between SavePageType constants and the indexes above.
const SavePageType kIndexToSaveType[] = {
  content::SAVE_PAGE_TYPE_UNKNOWN,
  content::SAVE_PAGE_TYPE_AS_ONLY_HTML,
  content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML,
};

int SavePackageTypeToIndex(SavePageType type) {
  for (size_t i = 0; i < arraysize(kIndexToSaveType); ++i) {
    if (kIndexToSaveType[i] == type)
      return i;
  }
  NOTREACHED();
  return -1;
}
#endif

// Indexes used for specifying which element in the extensions dropdown
// the user chooses when picking a save type.
const int kSelectFileHtmlOnlyIndex = 1;
const int kSelectFileCompleteIndex = 2;

// Used for mapping between the IDS_ string identifiers and the indexes above.
const int kIndexToIDS[] = {
  0, IDS_SAVE_PAGE_DESC_HTML_ONLY, IDS_SAVE_PAGE_DESC_COMPLETE,
};

void OnSavePackageDownloadCreated(content::DownloadItem* download) {
  ChromeDownloadManagerDelegate::DisableSafeBrowsing(download);
}

#if defined(OS_CHROMEOS)
void OnSavePackageDownloadCreatedChromeOS(
    Profile* profile,
    const base::FilePath& drive_path,
    content::DownloadItem* download) {
  drive::DownloadHandler::GetForProfile(profile)->SetDownloadParams(
      drive_path, download);
  OnSavePackageDownloadCreated(download);
}

// Trampoline callback between SubstituteDriveDownloadPath() and |callback|.
void ContinueSettingUpDriveDownload(
    const content::SavePackagePathPickedCallback& callback,
    content::SavePageType save_type,
    Profile* profile,
    const base::FilePath& drive_path,
    const base::FilePath& drive_tmp_download_path) {
  if (drive_tmp_download_path.empty())  // Substitution failed.
    return;
  callback.Run(drive_tmp_download_path, save_type, base::Bind(
      &OnSavePackageDownloadCreatedChromeOS, profile, drive_path));
}
#endif

}  // anonymous namespace

bool SavePackageFilePicker::ShouldSaveAsMHTML() const {
#if !defined(OS_CHROMEOS)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSavePageAsMHTML))
    return false;
#endif
  return can_save_as_complete_;
}

SavePackageFilePicker::SavePackageFilePicker(
    content::WebContents* web_contents,
    const base::FilePath& suggested_path,
    const base::FilePath::StringType& default_extension,
    bool can_save_as_complete,
    DownloadPrefs* download_prefs,
    const content::SavePackagePathPickedCallback& callback)
    : render_process_id_(web_contents->GetRenderProcessHost()->GetID()),
      can_save_as_complete_(can_save_as_complete),
      download_prefs_(download_prefs),
      callback_(callback) {
  base::FilePath suggested_path_copy = suggested_path;
  base::FilePath::StringType default_extension_copy = default_extension;
  int file_type_index = 0;
  ui::SelectFileDialog::FileTypeInfo file_type_info;

#if defined(OS_CHROMEOS)
  file_type_info.support_drive = true;
#else
  file_type_index = SavePackageTypeToIndex(
      static_cast<SavePageType>(download_prefs_->save_file_type()));
  DCHECK_NE(-1, file_type_index);
#endif

  // TODO(benjhayden): Merge the first branch with the second when all of the
  // platform-specific file selection dialog implementations fully support
  // switching save-as file formats, and remove the flag/switch.
  if (ShouldSaveAsMHTML()) {
    default_extension_copy = FILE_PATH_LITERAL("mhtml");
    suggested_path_copy = suggested_path_copy.ReplaceExtension(
        default_extension_copy);
  } else if (can_save_as_complete_) {
    // NOTE: this branch will never run on chromeos because ShouldSaveAsHTML()
    // == can_save_as_complete_ on chromeos.
    bool add_extra_extension = false;
    base::FilePath::StringType extra_extension;
    if (!suggested_path_copy.Extension().empty() &&
        !suggested_path_copy.MatchesExtension(FILE_PATH_LITERAL(".htm")) &&
        !suggested_path_copy.MatchesExtension(FILE_PATH_LITERAL(".html"))) {
      add_extra_extension = true;
      extra_extension = suggested_path_copy.Extension().substr(1);
    }

    static const size_t kNumberExtensions = arraysize(kIndexToIDS) - 1;
    file_type_info.extensions.resize(kNumberExtensions);
    file_type_info.extension_description_overrides.resize(kNumberExtensions);

    // Indices into kIndexToIDS are 1-based whereas indices into
    // file_type_info.extensions are 0-based. Hence the '-1's.
    // If you switch these resize()/direct-assignment patterns to push_back(),
    // then you risk breaking FileSelected()'s use of |index|.

    file_type_info.extension_description_overrides[
      kSelectFileHtmlOnlyIndex - 1] = l10n_util::GetStringUTF16(kIndexToIDS[
          kSelectFileHtmlOnlyIndex]);
    file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1].push_back(
        FILE_PATH_LITERAL("htm"));
    file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1].push_back(
        FILE_PATH_LITERAL("html"));
    if (add_extra_extension) {
      file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1].push_back(
          extra_extension);
    }

    file_type_info.extension_description_overrides[
      kSelectFileCompleteIndex - 1] = l10n_util::GetStringUTF16(kIndexToIDS[
          kSelectFileCompleteIndex]);
    file_type_info.extensions[kSelectFileCompleteIndex - 1].push_back(
        FILE_PATH_LITERAL("htm"));
    file_type_info.extensions[kSelectFileCompleteIndex - 1].push_back(
        FILE_PATH_LITERAL("html"));
    if (add_extra_extension) {
      file_type_info.extensions[kSelectFileCompleteIndex - 1].push_back(
          extra_extension);
    }

    file_type_info.include_all_files = false;
  } else {
    // The contents can not be saved as complete-HTML, so do not show the file
    // filters.
    file_type_info.extensions.resize(1);
    file_type_info.extensions[0].push_back(suggested_path_copy.Extension());

    if (!file_type_info.extensions[0][0].empty()) {
      // Drop the .
      file_type_info.extensions[0][0].erase(0, 1);
    }

    file_type_info.include_all_files = true;
    file_type_index = 1;
  }

  if (g_should_prompt_for_filename) {
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, new ChromeSelectFilePolicy(web_contents));
    select_file_dialog_->SelectFile(
        ui::SelectFileDialog::SELECT_SAVEAS_FILE,
        string16(),
        suggested_path_copy,
        &file_type_info,
        file_type_index,
        default_extension_copy,
        platform_util::GetTopLevel(web_contents->GetView()->GetNativeView()),
        NULL);
  } else {
    // Just use 'suggested_path_copy' instead of opening the dialog prompt.
    // Go through FileSelected() for consistency.
    FileSelected(suggested_path_copy, file_type_index, NULL);
  }
}

SavePackageFilePicker::~SavePackageFilePicker() {
}

void SavePackageFilePicker::SetShouldPromptUser(bool should_prompt) {
  g_should_prompt_for_filename = should_prompt;
}

void SavePackageFilePicker::FileSelected(
    const base::FilePath& path, int index, void* unused_params) {
  scoped_ptr<SavePackageFilePicker> delete_this(this);
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;
  SavePageType save_type = content::SAVE_PAGE_TYPE_UNKNOWN;

  if (ShouldSaveAsMHTML()) {
    save_type = content::SAVE_PAGE_TYPE_AS_MHTML;
  } else {
#if defined(OS_CHROMEOS)
    save_type = content::SAVE_PAGE_TYPE_AS_ONLY_HTML;
#else
    // The option index is not zero-based.
    DCHECK(index >= kSelectFileHtmlOnlyIndex &&
           index <= kSelectFileCompleteIndex);
    save_type = kIndexToSaveType[index];
    if (select_file_dialog_.get() &&
        select_file_dialog_->HasMultipleFileTypeChoices())
      download_prefs_->SetSaveFileType(save_type);
#endif
  }

  UMA_HISTOGRAM_ENUMERATION("Download.SavePageType",
                            save_type,
                            content::SAVE_PAGE_TYPE_MAX);

  base::FilePath path_copy(path);
  file_util::NormalizeFileNameEncoding(&path_copy);

  download_prefs_->SetSaveFilePath(path_copy.DirName());

#if defined(OS_CHROMEOS)
  if (drive::util::IsUnderDriveMountPoint(path_copy)) {
    // Here's a map to the callback chain:
    // SubstituteDriveDownloadPath ->
    //   ContinueSettingUpDriveDownload ->
    //     callback_ = SavePackage::OnPathPicked ->
    //       download_created_callback = OnSavePackageDownloadCreatedChromeOS
    Profile* profile = Profile::FromBrowserContext(
        process->GetBrowserContext());
    drive::DownloadHandler* drive_download_handler =
        drive::DownloadHandler::GetForProfile(profile);
    drive_download_handler->SubstituteDriveDownloadPath(
        path_copy, NULL, base::Bind(&ContinueSettingUpDriveDownload,
                                    callback_,
                                    save_type,
                                    profile,
                                    path_copy));
    return;
  }
#endif

  callback_.Run(path_copy, save_type,
                base::Bind(&OnSavePackageDownloadCreated));
}

void SavePackageFilePicker::FileSelectionCanceled(void* unused_params) {
  delete this;
}
