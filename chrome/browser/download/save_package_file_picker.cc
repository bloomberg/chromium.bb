// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/save_package_file_picker.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::RenderProcessHost;
using content::SavePageType;
using content::WebContents;

namespace {

// If false, we don't prompt the user as to where to save the file.  This
// exists only for testing.
bool g_should_prompt_for_filename = true;

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

// Indexes used for specifying which element in the extensions dropdown
// the user chooses when picking a save type.
const int kSelectFileHtmlOnlyIndex = 1;
const int kSelectFileCompleteIndex = 2;

// Used for mapping between the IDS_ string identifiers and the indexes above.
const int kIndexToIDS[] = {
  0, IDS_SAVE_PAGE_DESC_HTML_ONLY, IDS_SAVE_PAGE_DESC_COMPLETE,
};

void OnSavePackageDownloadCreated(
    content::DownloadItem* download) {
  ChromeDownloadManagerDelegate::DisableSafeBrowsing(download);
}

}  // anonymous namespace

bool SavePackageFilePicker::ShouldSaveAsMHTML() const {
  return can_save_as_complete_ &&
         CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSavePageAsMHTML);
}

SavePackageFilePicker::SavePackageFilePicker(
    content::WebContents* web_contents,
    const FilePath& suggested_path_const,
    const FilePath::StringType& default_extension_const,
    bool can_save_as_complete,
    DownloadPrefs* download_prefs,
    const content::SavePackagePathPickedCallback& callback)
    : render_process_id_(web_contents->GetRenderProcessHost()->GetID()),
      can_save_as_complete_(can_save_as_complete),
      callback_(callback) {
  FilePath suggested_path = suggested_path_const;
  FilePath::StringType default_extension = default_extension_const;
  int file_type_index = SavePackageTypeToIndex(
      static_cast<SavePageType>(download_prefs->save_file_type()));
  DCHECK_NE(-1, file_type_index);

  SelectFileDialog::FileTypeInfo file_type_info;

  // TODO(benjhayden): Merge the first branch with the second when all of the
  // platform-specific file selection dialog implementations fully support
  // switching save-as file formats, and remove the flag/switch.
  if (ShouldSaveAsMHTML()) {
    default_extension = FILE_PATH_LITERAL("mhtml");
    suggested_path = suggested_path.ReplaceExtension(default_extension);
  } else if (can_save_as_complete) {
    bool add_extra_extension = false;
    FilePath::StringType extra_extension;
    if (!suggested_path.Extension().empty() &&
        suggested_path.Extension().compare(FILE_PATH_LITERAL("htm")) &&
        suggested_path.Extension().compare(FILE_PATH_LITERAL("html"))) {
      add_extra_extension = true;
      extra_extension = suggested_path.Extension().substr(1);
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
    file_type_info.extensions[0].push_back(
        suggested_path.Extension());

    if (!file_type_info.extensions[0][0].empty()) {
      // Drop the .
      file_type_info.extensions[0][0].erase(0, 1);
    }

    file_type_info.include_all_files = true;
    file_type_index = 1;
  }

  if (g_should_prompt_for_filename) {
    select_file_dialog_ = SelectFileDialog::Create(this);
    select_file_dialog_->SelectFile(SelectFileDialog::SELECT_SAVEAS_FILE,
                                    string16(),
                                    suggested_path,
                                    &file_type_info,
                                    file_type_index,
                                    default_extension,
                                    web_contents,
                                    platform_util::GetTopLevel(
                                        web_contents->GetNativeView()),
                                    NULL);
  } else {
    // Just use 'suggested_path' instead of opening the dialog prompt.
    // Go through FileSelected() for consistency.
    FileSelected(suggested_path, file_type_index, NULL);
  }
}

SavePackageFilePicker::~SavePackageFilePicker() {
}

void SavePackageFilePicker::SetShouldPromptUser(bool should_prompt) {
  g_should_prompt_for_filename = should_prompt;
}

void SavePackageFilePicker::FileSelected(const FilePath& path,
                                         int index,
                                         void* unused_params) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (process) {
    SavePageType save_type = content::SAVE_PAGE_TYPE_UNKNOWN;
    PrefService* prefs = Profile::FromBrowserContext(
        process->GetBrowserContext())->GetPrefs();
    if (ShouldSaveAsMHTML()) {
      save_type = content::SAVE_PAGE_TYPE_AS_MHTML;
    } else {
      // The option index is not zero-based.
      DCHECK(index >= kSelectFileHtmlOnlyIndex &&
             index <= kSelectFileCompleteIndex);
      save_type = kIndexToSaveType[index];
      if (select_file_dialog_ &&
          select_file_dialog_->HasMultipleFileTypeChoices())
        prefs->SetInteger(prefs::kSaveFileType, save_type);
    }

    UMA_HISTOGRAM_ENUMERATION("Download.SavePageType",
                              save_type,
                              content::SAVE_PAGE_TYPE_MAX);

    StringPrefMember save_file_path;
    save_file_path.Init(prefs::kSaveFileDefaultDirectory, prefs, NULL);
#if defined(OS_POSIX)
    std::string path_string = path.DirName().value();
#elif defined(OS_WIN)
    std::string path_string = WideToUTF8(path.DirName().value());
#endif
    // If user change the default saving directory, we will remember it just
    // like IE and FireFox.
    if (!process->GetBrowserContext()->IsOffTheRecord() &&
        save_file_path.GetValue() != path_string)
      save_file_path.SetValue(path_string);

    callback_.Run(path, save_type, base::Bind(&OnSavePackageDownloadCreated));
  }

  delete this;
}

void SavePackageFilePicker::FileSelectionCanceled(void* unused_params) {
  delete this;
}
