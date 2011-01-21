// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_select_helper.h"

#include <string>

#include "base/file_util.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/mime_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/render_messages_params.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

FileSelectHelper::FileSelectHelper(Profile* profile)
    : profile_(profile),
      render_view_host_(NULL),
      select_file_dialog_(),
      dialog_type_(SelectFileDialog::SELECT_OPEN_FILE) {
}

FileSelectHelper::~FileSelectHelper() {
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

void FileSelectHelper::FileSelected(const FilePath& path,
                                    int index, void* params) {
  if (!render_view_host_)
    return;

  profile_->set_last_selected_directory(path.DirName());

  if (dialog_type_ == SelectFileDialog::SELECT_FOLDER) {
    DirectorySelected(path);
    return;
  }

  std::vector<FilePath> files;
  files.push_back(path);
  render_view_host_->FilesSelectedInChooser(files);
  // We are done with this showing of the dialog.
  render_view_host_ = NULL;
}

void FileSelectHelper::MultiFilesSelected(const std::vector<FilePath>& files,
                                          void* params) {
  if (!files.empty())
    profile_->set_last_selected_directory(files[0].DirName());
  if (!render_view_host_)
    return;

  render_view_host_->FilesSelectedInChooser(files);
  // We are done with this showing of the dialog.
  render_view_host_ = NULL;
}

void FileSelectHelper::FileSelectionCanceled(void* params) {
  if (!render_view_host_)
    return;

  // If the user cancels choosing a file to upload we pass back an
  // empty vector.
  render_view_host_->FilesSelectedInChooser(std::vector<FilePath>());

  // We are done with this showing of the dialog.
  render_view_host_ = NULL;
}

void FileSelectHelper::DirectorySelected(const FilePath& path) {
  directory_lister_ = new net::DirectoryLister(path,
                                               true,
                                               net::DirectoryLister::NO_SORT,
                                               this);
  if (!directory_lister_->Start())
    FileSelectionCanceled(NULL);
}

void FileSelectHelper::OnListFile(
    const net::DirectoryLister::DirectoryListerData& data) {
  // Directory upload only cares about files.  This util call just checks
  // the flags in the structure; there's no file I/O going on.
  if (file_util::FileEnumerator::IsDirectory(data.info))
    return;

  directory_lister_results_.push_back(data.path);
}

void FileSelectHelper::OnListDone(int error) {
  if (!render_view_host_)
    return;

  if (error) {
    FileSelectionCanceled(NULL);
    return;
  }

  render_view_host_->FilesSelectedInChooser(directory_lister_results_);
  render_view_host_ = NULL;
  directory_lister_ = NULL;
  directory_lister_results_.clear();
}

SelectFileDialog::FileTypeInfo* FileSelectHelper::GetFileTypesFromAcceptType(
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

  // If no valid extension is added, bail out.
  if (valid_type_count == 0)
    return NULL;

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

void FileSelectHelper::RunFileChooser(
    RenderViewHost* render_view_host,
    const ViewHostMsg_RunFileChooser_Params &params) {
  DCHECK(!render_view_host_);
  render_view_host_ = render_view_host;
  notification_registrar_.RemoveAll();
  notification_registrar_.Add(this,
                              NotificationType::RENDER_WIDGET_HOST_DESTROYED,
                              Source<RenderViewHost>(render_view_host));

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
    default_file_name = profile_->last_selected_directory();

  gfx::NativeWindow owning_window =
      platform_util::GetTopLevel(render_view_host_->view()->GetNativeView());
  select_file_dialog_->SelectFile(dialog_type_,
                                  params.title,
                                  default_file_name,
                                  file_types.get(),
                                  file_types.get() ? 1 : 0,  // 1-based index.
                                  FILE_PATH_LITERAL(""),
                                  owning_window,
                                  NULL);
}

void FileSelectHelper::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  DCHECK(type == NotificationType::RENDER_WIDGET_HOST_DESTROYED);
  DCHECK(Details<RenderViewHost>(details).ptr() == render_view_host_);
  render_view_host_ = NULL;
}
