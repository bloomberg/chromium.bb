// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents_file_select_helper.h"

#include "app/l10n_util.h"
#include "base/file_util.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/mime_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/render_messages_params.h"
#include "grit/generated_resources.h"

TabContentsFileSelectHelper::TabContentsFileSelectHelper(
    TabContents* tab_contents)
    : tab_contents_(tab_contents),
      select_file_dialog_(),
      dialog_type_(SelectFileDialog::SELECT_OPEN_FILE) {
}

TabContentsFileSelectHelper::~TabContentsFileSelectHelper() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  // Stop any pending directory enumeration and prevent a callback.
  if (directory_lister_.get()) {
    directory_lister_->set_delegate(NULL);
    directory_lister_->Cancel();
  }
}

void TabContentsFileSelectHelper::FileSelected(const FilePath& path,
                                               int index, void* params) {
  tab_contents_->profile()->set_last_selected_directory(path.DirName());

  if (dialog_type_ == SelectFileDialog::SELECT_FOLDER) {
    DirectorySelected(path);
    return;
  }

  std::vector<FilePath> files;
  files.push_back(path);
  GetRenderViewHost()->FilesSelectedInChooser(files);
}

void TabContentsFileSelectHelper::MultiFilesSelected(
    const std::vector<FilePath>& files, void* params) {
  if (!files.empty())
    tab_contents_->profile()->set_last_selected_directory(files[0].DirName());
  GetRenderViewHost()->FilesSelectedInChooser(files);
}

void TabContentsFileSelectHelper::DirectorySelected(const FilePath& path) {
  directory_lister_ = new net::DirectoryLister(path,
                                               true,
                                               net::DirectoryLister::NO_SORT,
                                               this);
  if (!directory_lister_->Start())
    FileSelectionCanceled(NULL);
}

void TabContentsFileSelectHelper::OnListFile(
    const net::DirectoryLister::DirectoryListerData& data) {
  // Directory upload only cares about files.  This util call just checks
  // the flags in the structure; there's no file I/O going on.
  if (file_util::FileEnumerator::IsDirectory(data.info))
    return;

  directory_lister_results_.push_back(data.path);
}

void TabContentsFileSelectHelper::OnListDone(int error) {
  if (error) {
    FileSelectionCanceled(NULL);
    return;
  }

  GetRenderViewHost()->FilesSelectedInChooser(directory_lister_results_);
  directory_lister_ = NULL;
}

void TabContentsFileSelectHelper::FileSelectionCanceled(void* params) {
  // If the user cancels choosing a file to upload we pass back an
  // empty vector.
  GetRenderViewHost()->FilesSelectedInChooser(std::vector<FilePath>());
}

SelectFileDialog::FileTypeInfo*
TabContentsFileSelectHelper::GetFileTypesFromAcceptType(
    const string16& accept_types) {
  if (accept_types.empty())
    return NULL;

  // Split the accept-type string on commas.
  std::vector<string16> mime_types;
  base::SplitStringUsingSubstr(accept_types, ASCIIToUTF16(","), &mime_types);
  if (mime_types.empty())
    return NULL;

  // Create FileTypeInfo and pre-allocate for the first extension list.
  scoped_ptr<SelectFileDialog::FileTypeInfo> file_type(
      new SelectFileDialog::FileTypeInfo());
  file_type->include_all_files = true;
  file_type->extensions.resize(1);
  std::vector<FilePath::StringType>* extensions = &file_type->extensions.back();

  // Find the correspondinge extensions.
  int valid_type_count = 0;
  int description_id = 0;
  for (size_t i = 0; i < mime_types.size(); ++i) {
    string16 mime_type = mime_types[i];
    std::string ascii_mime_type = StringToLowerASCII(UTF16ToASCII(mime_type));

    TrimWhitespace(ascii_mime_type, TRIM_ALL, &ascii_mime_type);
    if (ascii_mime_type.empty())
      continue;

    size_t old_extension_size = extensions->size();
    if (ascii_mime_type == "image/*") {
      description_id = IDS_IMAGE_FILES;
      net::GetImageExtensions(extensions);
    } else if (ascii_mime_type == "audio/*") {
      description_id = IDS_AUDIO_FILES;
      net::GetAudioExtensions(extensions);
    } else if (ascii_mime_type == "video/*") {
      description_id = IDS_VIDEO_FILES;
      net::GetVideoExtensions(extensions);
    } else {
      net::GetExtensionsForMimeType(ascii_mime_type, extensions);
    }

    if (extensions->size() > old_extension_size)
      valid_type_count++;
  }

  // Use a generic description "Custom Files" if either of the following is
  // true:
  // 1) There're multiple types specified, like "audio/*,video/*"
  // 2) There're multiple extensions for a MIME type without parameter, like
  //    "ehtml,shtml,htm,html" for "text/html". On Windows, the select file
  //    dialog uses the first extension in the list to form the description,
  //    like "EHTML Files". This is not what we want.
  if (valid_type_count > 1 ||
      (valid_type_count == 1 && description_id == 0 && extensions->size() > 1))
    description_id = IDS_CUSTOM_FILES;

  if (description_id) {
    file_type->extension_description_overrides.push_back(
        l10n_util::GetStringUTF16(description_id));
  }

  return file_type.release();
}

void TabContentsFileSelectHelper::RunFileChooser(
    const ViewHostMsg_RunFileChooser_Params &params) {
  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);

  switch (params.mode) {
    case ViewHostMsg_RunFileChooser_Params::Open:
      dialog_type_ = SelectFileDialog::SELECT_OPEN_FILE;
      break;
    case ViewHostMsg_RunFileChooser_Params::OpenMultiple:
      dialog_type_ = SelectFileDialog::SELECT_OPEN_MULTI_FILE;
      break;
    case ViewHostMsg_RunFileChooser_Params::OpenFolder:
      dialog_type_ = SelectFileDialog::SELECT_FOLDER;
      break;
    case ViewHostMsg_RunFileChooser_Params::Save:
      dialog_type_ = SelectFileDialog::SELECT_SAVEAS_FILE;
      break;
    default:
      dialog_type_ = SelectFileDialog::SELECT_OPEN_FILE;  // Prevent warning.
      NOTREACHED();
  }
  scoped_ptr<SelectFileDialog::FileTypeInfo> file_types(
      GetFileTypesFromAcceptType(params.accept_types));
  FilePath default_file_name = params.default_file_name;
  if (default_file_name.empty())
    default_file_name = tab_contents_->profile()->last_selected_directory();
  select_file_dialog_->SelectFile(
      dialog_type_,
      params.title,
      default_file_name,
      file_types.get(),
      0,
      FILE_PATH_LITERAL(""),
      tab_contents_->view()->GetTopLevelNativeWindow(),
      NULL);
}

RenderViewHost* TabContentsFileSelectHelper::GetRenderViewHost() {
  return tab_contents_->render_view_host();
}
