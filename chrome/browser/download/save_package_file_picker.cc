// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/save_package_file_picker.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/browser/download/save_package.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// If false, we don't prompt the user as to where to save the file.  This
// exists only for testing.
bool g_should_prompt_for_filename = true;

// Used for mapping between SavePackageType constants and the indexes above.
const SavePackage::SavePackageType kIndexToSaveType[] = {
  SavePackage::SAVE_TYPE_UNKNOWN,
  SavePackage::SAVE_AS_ONLY_HTML,
  SavePackage::SAVE_AS_COMPLETE_HTML,
};

int SavePackageTypeToIndex(SavePackage::SavePackageType type) {
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

}

SavePackageFilePicker::SavePackageFilePicker(
    const base::WeakPtr<SavePackage>& save_package,
    const FilePath& suggested_path,
    bool can_save_as_complete)
    : save_package_(save_package) {
  DownloadPrefs* download_prefs = save_package->tab_contents()->
      browser_context()->GetDownloadManager()->download_prefs();
  int file_type_index = SavePackageTypeToIndex(
      static_cast<SavePackage::SavePackageType>(
          download_prefs->save_file_type()));
  DCHECK_NE(-1, file_type_index);

  SelectFileDialog::FileTypeInfo file_type_info;
  FilePath::StringType default_extension;

  // If the contents can not be saved as complete-HTML, do not show the
  // file filters.
  if (can_save_as_complete) {
    bool add_extra_extension = false;
    FilePath::StringType extra_extension;
    if (!suggested_path.Extension().empty() &&
        suggested_path.Extension().compare(FILE_PATH_LITERAL("htm")) &&
        suggested_path.Extension().compare(FILE_PATH_LITERAL("html"))) {
      add_extra_extension = true;
      extra_extension = suggested_path.Extension().substr(1);
    }

    file_type_info.extensions.resize(2);
    file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1].push_back(
        FILE_PATH_LITERAL("htm"));
    file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1].push_back(
        FILE_PATH_LITERAL("html"));

    if (add_extra_extension) {
      file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1].push_back(
          extra_extension);
    }

    file_type_info.extension_description_overrides.push_back(
        l10n_util::GetStringUTF16(kIndexToIDS[kSelectFileCompleteIndex - 1]));
    file_type_info.extensions[kSelectFileCompleteIndex - 1].push_back(
        FILE_PATH_LITERAL("htm"));
    file_type_info.extensions[kSelectFileCompleteIndex - 1].push_back(
        FILE_PATH_LITERAL("html"));

    if (add_extra_extension) {
      file_type_info.extensions[kSelectFileCompleteIndex - 1].push_back(
          extra_extension);
    }

    file_type_info.extension_description_overrides.push_back(
        l10n_util::GetStringUTF16(kIndexToIDS[kSelectFileCompleteIndex]));
    file_type_info.include_all_files = false;
    default_extension = SavePackage::kDefaultHtmlExtension;
  } else {
    file_type_info.extensions.resize(1);
    file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1].push_back(
        suggested_path.Extension());

    if (!file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1][0].empty()) {
      // Drop the .
      file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1][0].erase(0, 1);
    }

    file_type_info.include_all_files = true;
    file_type_index = 1;
  }

  if (g_should_prompt_for_filename) {
    select_file_dialog_ = SelectFileDialog::Create(this);
    TabContents* tab_contents = save_package_->tab_contents();
    select_file_dialog_->SelectFile(SelectFileDialog::SELECT_SAVEAS_FILE,
                                    string16(),
                                    suggested_path,
                                    &file_type_info,
                                    file_type_index,
                                    default_extension,
                                    tab_contents,
                                    platform_util::GetTopLevel(
                                        tab_contents->GetNativeView()),
                                    NULL);
  } else {
    // Just use 'suggested_path' instead of opening the dialog prompt.
    save_package_->OnPathPicked(
        suggested_path, kIndexToSaveType[file_type_index]);
  }
}

SavePackageFilePicker::~SavePackageFilePicker() {
}

void SavePackageFilePicker::SetShouldPromptUser(bool should_prompt) {
  g_should_prompt_for_filename = should_prompt;
}

void SavePackageFilePicker::FileSelected(const FilePath& path,
                                         int index,
                                         void* params) {
  // The option index is not zero-based.
  DCHECK(index >= kSelectFileHtmlOnlyIndex &&
         index <= kSelectFileCompleteIndex);

  if (save_package_) {
    TabContents* tab_contents = save_package_->tab_contents();
    SavePackage::SavePackageType save_type = kIndexToSaveType[index];
    Profile* profile =
        Profile::FromBrowserContext(tab_contents->browser_context());
    PrefService* prefs = profile->GetPrefs();
    prefs->SetInteger(prefs::kSaveFileType, save_type);

    StringPrefMember save_file_path;
    save_file_path.Init(prefs::kSaveFileDefaultDirectory, prefs, NULL);
#if defined(OS_POSIX)
    std::string path_string = path.DirName().value();
#elif defined(OS_WIN)
    std::string path_string = WideToUTF8(path.DirName().value());
#endif
    // If user change the default saving directory, we will remember it just
    // like IE and FireFox.
    if (!tab_contents->browser_context()->IsOffTheRecord() &&
        save_file_path.GetValue() != path_string) {
      save_file_path.SetValue(path_string);
    }

    save_package_->OnPathPicked(path, save_type);
  }

  delete this;
}

void SavePackageFilePicker::FileSelectionCanceled(void* params) {
  delete this;
}
