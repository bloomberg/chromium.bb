// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_select_helper.h"

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/platform_file.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/file_chooser_params.h"
#include "grit/generated_resources.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

// There is only one file-selection happening at any given time,
// so we allocate an enumeration ID for that purpose.  All IDs from
// the renderer must start at 0 and increase.
const int kFileSelectEnumerationId = -1;

void NotifyRenderViewHost(RenderViewHost* render_view_host,
                          const std::vector<FilePath>& files,
                          SelectFileDialog::Type dialog_type) {
  const int kReadFilePermissions =
      base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_EXCLUSIVE_READ |
      base::PLATFORM_FILE_ASYNC;

  const int kWriteFilePermissions =
      base::PLATFORM_FILE_CREATE |
      base::PLATFORM_FILE_CREATE_ALWAYS |
      base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_OPEN_ALWAYS |
      base::PLATFORM_FILE_OPEN_TRUNCATED |
      base::PLATFORM_FILE_WRITE |
      base::PLATFORM_FILE_WRITE_ATTRIBUTES |
      base::PLATFORM_FILE_ASYNC;

  int permissions = kReadFilePermissions;
  if (dialog_type == SelectFileDialog::SELECT_SAVEAS_FILE)
    permissions = kWriteFilePermissions;
  render_view_host->FilesSelectedInChooser(files, permissions);
}
}

struct FileSelectHelper::ActiveDirectoryEnumeration {
  ActiveDirectoryEnumeration() : rvh_(NULL) {}

  scoped_ptr<DirectoryListerDispatchDelegate> delegate_;
  scoped_ptr<net::DirectoryLister> lister_;
  RenderViewHost* rvh_;
  std::vector<FilePath> results_;
};

FileSelectHelper::FileSelectHelper(Profile* profile)
    : profile_(profile),
      render_view_host_(NULL),
      tab_contents_(NULL),
      select_file_dialog_(),
      select_file_types_(),
      dialog_type_(SelectFileDialog::SELECT_OPEN_FILE) {
}

FileSelectHelper::~FileSelectHelper() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  // Stop any pending directory enumeration, prevent a callback, and free
  // allocated memory.
  std::map<int, ActiveDirectoryEnumeration*>::iterator iter;
  for (iter = directory_enumerations_.begin();
       iter != directory_enumerations_.end();
       ++iter) {
    iter->second->lister_.reset();
    delete iter->second;
  }
}

void FileSelectHelper::FileSelected(const FilePath& path,
                                    int index, void* params) {
  if (!render_view_host_)
    return;

  profile_->set_last_selected_directory(path.DirName());

  if (dialog_type_ == SelectFileDialog::SELECT_FOLDER) {
    StartNewEnumeration(path, kFileSelectEnumerationId, render_view_host_);
    return;
  }

  std::vector<FilePath> files;
  files.push_back(path);
  NotifyRenderViewHost(render_view_host_, files, dialog_type_);

  // No members should be accessed from here on.
  RunFileChooserEnd();
}

void FileSelectHelper::MultiFilesSelected(const std::vector<FilePath>& files,
                                          void* params) {
  if (!files.empty())
    profile_->set_last_selected_directory(files[0].DirName());
  if (!render_view_host_)
    return;

  NotifyRenderViewHost(render_view_host_, files, dialog_type_);

  // No members should be accessed from here on.
  RunFileChooserEnd();
}

void FileSelectHelper::FileSelectionCanceled(void* params) {
  if (!render_view_host_)
    return;

  // If the user cancels choosing a file to upload we pass back an
  // empty vector.
  NotifyRenderViewHost(
      render_view_host_, std::vector<FilePath>(), dialog_type_);

  // No members should be accessed from here on.
  RunFileChooserEnd();
}

void FileSelectHelper::StartNewEnumeration(const FilePath& path,
                                           int request_id,
                                           RenderViewHost* render_view_host) {
  scoped_ptr<ActiveDirectoryEnumeration> entry(new ActiveDirectoryEnumeration);
  entry->rvh_ = render_view_host;
  entry->delegate_.reset(new DirectoryListerDispatchDelegate(this, request_id));
  entry->lister_.reset(new net::DirectoryLister(path,
                                                true,
                                                net::DirectoryLister::NO_SORT,
                                                entry->delegate_.get()));
  if (!entry->lister_->Start()) {
    if (request_id == kFileSelectEnumerationId)
      FileSelectionCanceled(NULL);
    else
      render_view_host->DirectoryEnumerationFinished(request_id,
                                                     entry->results_);
  } else {
    directory_enumerations_[request_id] = entry.release();
  }
}

void FileSelectHelper::OnListFile(
    int id,
    const net::DirectoryLister::DirectoryListerData& data) {
  ActiveDirectoryEnumeration* entry = directory_enumerations_[id];

  // Directory upload returns directories via a "." file, so that
  // empty directories are included.  This util call just checks
  // the flags in the structure; there's no file I/O going on.
  if (file_util::FileEnumerator::IsDirectory(data.info))
    entry->results_.push_back(data.path.Append(FILE_PATH_LITERAL(".")));
  else
    entry->results_.push_back(data.path);
}

void FileSelectHelper::OnListDone(int id, int error) {
  // This entry needs to be cleaned up when this function is done.
  scoped_ptr<ActiveDirectoryEnumeration> entry(directory_enumerations_[id]);
  directory_enumerations_.erase(id);
  if (!entry->rvh_)
    return;
  if (error) {
    FileSelectionCanceled(NULL);
    return;
  }
  if (id == kFileSelectEnumerationId)
    NotifyRenderViewHost(entry->rvh_, entry->results_, dialog_type_);
  else
    entry->rvh_->DirectoryEnumerationFinished(id, entry->results_);

  EnumerateDirectoryEnd();
}

SelectFileDialog::FileTypeInfo* FileSelectHelper::GetFileTypesFromAcceptType(
    const std::vector<string16>& accept_types) {
  if (accept_types.empty())
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
  for (size_t i = 0; i < accept_types.size(); ++i) {
    std::string ascii_mime_type = UTF16ToASCII(accept_types[i]);
    // WebKit normalizes MIME types.  See HTMLInputElement::acceptMIMETypes().
    DCHECK(StringToLowerASCII(ascii_mime_type) == ascii_mime_type)
        << "A MIME type contains uppercase letter: " << ascii_mime_type;
    DCHECK(TrimWhitespaceASCII(ascii_mime_type, TRIM_ALL, &ascii_mime_type)
        == TRIM_NONE)
        << "A MIME type contains whitespace: '" << ascii_mime_type << "'";

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
    TabContents* tab_contents,
    const content::FileChooserParams& params) {
  DCHECK(!render_view_host_);
  DCHECK(!tab_contents_);
  render_view_host_ = render_view_host;
  tab_contents_ = tab_contents;
  notification_registrar_.RemoveAll();
  notification_registrar_.Add(
      this, content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      content::Source<RenderWidgetHost>(render_view_host_));
  notification_registrar_.Add(
      this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
      content::Source<TabContents>(tab_contents_));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&FileSelectHelper::RunFileChooserOnFileThread, this, params));

  // Because this class returns notifications to the RenderViewHost, it is
  // difficult for callers to know how long to keep a reference to this
  // instance. We AddRef() here to keep the instance alive after we return
  // to the caller, until the last callback is received from the file dialog.
  // At that point, we must call RunFileChooserEnd().
  AddRef();
}

void FileSelectHelper::RunFileChooserOnFileThread(
    const content::FileChooserParams& params) {
  select_file_types_.reset(
      GetFileTypesFromAcceptType(params.accept_types));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&FileSelectHelper::RunFileChooserOnUIThread, this, params));
}

void FileSelectHelper::RunFileChooserOnUIThread(
    const content::FileChooserParams& params) {
  if (!render_view_host_ || !tab_contents_)
    return;

  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);

  switch (params.mode) {
    case content::FileChooserParams::Open:
      dialog_type_ = SelectFileDialog::SELECT_OPEN_FILE;
      break;
    case content::FileChooserParams::OpenMultiple:
      dialog_type_ = SelectFileDialog::SELECT_OPEN_MULTI_FILE;
      break;
    case content::FileChooserParams::OpenFolder:
      dialog_type_ = SelectFileDialog::SELECT_FOLDER;
      break;
    case content::FileChooserParams::Save:
      dialog_type_ = SelectFileDialog::SELECT_SAVEAS_FILE;
      break;
    default:
      dialog_type_ = SelectFileDialog::SELECT_OPEN_FILE;  // Prevent warning.
      NOTREACHED();
  }
  FilePath default_file_name = params.default_file_name;
  if (default_file_name.empty())
    default_file_name = profile_->last_selected_directory();

  gfx::NativeWindow owning_window =
      platform_util::GetTopLevel(render_view_host_->view()->GetNativeView());

  select_file_dialog_->SelectFile(
      dialog_type_,
      params.title,
      default_file_name,
      select_file_types_.get(),
      select_file_types_.get() ? 1 : 0,  // 1-based index.
      FILE_PATH_LITERAL(""),
      tab_contents_,
      owning_window,
      NULL);

  select_file_types_.reset();
}

// This method is called when we receive the last callback from the file
// chooser dialog. Perform any cleanup and release the reference we added
// in RunFileChooser().
void FileSelectHelper::RunFileChooserEnd() {
  render_view_host_ = NULL;
  tab_contents_ = NULL;
  Release();
}

void FileSelectHelper::EnumerateDirectory(int request_id,
                                          RenderViewHost* render_view_host,
                                          const FilePath& path) {
  DCHECK_NE(kFileSelectEnumerationId, request_id);

  // Because this class returns notifications to the RenderViewHost, it is
  // difficult for callers to know how long to keep a reference to this
  // instance. We AddRef() here to keep the instance alive after we return
  // to the caller, until the last callback is received from the enumeration
  // code. At that point, we must call EnumerateDirectoryEnd().
  AddRef();
  StartNewEnumeration(path, request_id, render_view_host);
}

// This method is called when we receive the last callback from the enumeration
// code. Perform any cleanup and release the reference we added in
// EnumerateDirectory().
void FileSelectHelper::EnumerateDirectoryEnd() {
  Release();
}

void FileSelectHelper::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED: {
      DCHECK(content::Source<RenderWidgetHost>(source).ptr() ==
             render_view_host_);
      render_view_host_ = NULL;
      break;
    }

    case content::NOTIFICATION_TAB_CONTENTS_DESTROYED: {
      DCHECK(content::Source<TabContents>(source).ptr() == tab_contents_);
      tab_contents_ = NULL;
      break;
    }

    default:
      NOTREACHED();
  }
}

