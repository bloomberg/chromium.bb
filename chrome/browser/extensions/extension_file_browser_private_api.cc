// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_file_browser_private_api.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_types.h"
#include "ui/base/l10n/l10n_util.h"


class LocalFileSystemCallbackDispatcher
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  explicit LocalFileSystemCallbackDispatcher(
      RequestLocalFileSystemFunction* function) : function_(function) {
    DCHECK(function_);
  }
  // fileapi::FileSystemCallbackDispatcher overrides.
  virtual void DidSucceed() OVERRIDE {
    NOTREACHED();
  }
  virtual void DidReadMetadata(const base::PlatformFileInfo& info,
                               const FilePath& unused) OVERRIDE {
    NOTREACHED();
  }
  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) OVERRIDE {
    NOTREACHED();
  }
  virtual void DidWrite(int64 bytes, bool complete) OVERRIDE {
    NOTREACHED();
  }
  virtual void DidOpenFileSystem(const std::string& name,
                                 const FilePath& path) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(function_,
            &RequestLocalFileSystemFunction::RespondSuccessOnUIThread,
            name,
            path));
  }
  virtual void DidFail(base::PlatformFileError error_code) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(function_,
            &RequestLocalFileSystemFunction::RespondFailedOnUIThread,
            error_code));
  }
 private:
  RequestLocalFileSystemFunction* function_;
  DISALLOW_COPY_AND_ASSIGN(LocalFileSystemCallbackDispatcher);
};

RequestLocalFileSystemFunction::RequestLocalFileSystemFunction() {
}

RequestLocalFileSystemFunction::~RequestLocalFileSystemFunction() {
}

bool RequestLocalFileSystemFunction::RunImpl() {
  fileapi::FileSystemOperation* operation =
      new fileapi::FileSystemOperation(
          new LocalFileSystemCallbackDispatcher(this),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          profile()->GetFileSystemContext(),
          NULL);
  GURL origin_url = source_url().GetOrigin();
  operation->OpenFileSystem(origin_url, fileapi::kFileSystemTypeLocal,
                            false);     // create
  // Will finish asynchronously.
  return true;
}

void RequestLocalFileSystemFunction::RespondSuccessOnUIThread(
    const std::string& name, const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  result_.reset(new DictionaryValue());
  DictionaryValue* dict = reinterpret_cast<DictionaryValue*>(result_.get());
  dict->SetString("name", name);
  dict->SetString("path", path.value());
  dict->SetInteger("error", base::PLATFORM_FILE_OK);
  SendResponse(true);
}

void RequestLocalFileSystemFunction::RespondFailedOnUIThread(
    base::PlatformFileError error_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  result_.reset(new DictionaryValue());
  DictionaryValue* dict = reinterpret_cast<DictionaryValue*>(result_.get());
  dict->SetInteger("error", static_cast<int>(error_code));
  SendResponse(true);
}

FileDialogFunction::FileDialogFunction() {
}

FileDialogFunction::~FileDialogFunction() {
}

// static
FileDialogFunction::ListenerMap FileDialogFunction::listener_map_;

// static
void FileDialogFunction::AddListener(int32 tab_id,
                                     SelectFileDialog::Listener* l) {
  if (listener_map_.find(tab_id) == listener_map_.end()) {
    listener_map_.insert(std::make_pair(tab_id, l));
  } else {
    DLOG_ASSERT("FileDialogFunction::AddListener tab_id already present");
  }
}

// static
void FileDialogFunction::RemoveListener(int32 tab_id) {
  listener_map_.erase(tab_id);
}

int32 FileDialogFunction::GetTabId() const {
  return dispatcher()->delegate()->associated_tab_contents()->
    controller().session_id().id();
}

SelectFileDialog::Listener* FileDialogFunction::GetListener() const {
  ListenerMap::const_iterator it = listener_map_.find(GetTabId());
  return (it == listener_map_.end()) ? NULL : it->second;
}

// GetFileSystemRootPathOnFileThread can only be called from the file thread,
// so here we are. This function takes a vector of virtual paths, converts
// them to local paths and calls GetLocalPathsResponseOnUIThread with the
// result vector, on the UI thread.
void FileDialogFunction::GetLocalPathsOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(selected_files_.empty());

  // FilePath(virtual_path) doesn't work on win, so limit this to ChromeOS.
#if defined(OS_CHROMEOS)
  GURL origin_url = source_url().GetOrigin();
  fileapi::FileSystemPathManager* path_manager =
      profile()->GetFileSystemContext()->path_manager();

  size_t len = virtual_paths_.size();
  selected_files_.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    std::string virtual_path = virtual_paths_[i];
    FilePath root = path_manager->GetFileSystemRootPathOnFileThread(
        origin_url,
        fileapi::kFileSystemTypeLocal,
        FilePath(virtual_path),
        false);
    if (!root.empty()) {
      selected_files_.push_back(root.Append(virtual_path));
    } else {
      LOG(WARNING) << "GetLocalPathsOnFileThread failed " << virtual_path;
    }
  }
#endif

  if (!selected_files_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
            &FileDialogFunction::GetLocalPathsResponseOnUIThread));
  }
}

bool SelectFileFunction::RunImpl() {
  DCHECK_EQ(static_cast<size_t>(2), args_->GetSize());

  std::string virtual_path;
  args_->GetString(0, &virtual_path);
  virtual_paths_.push_back(virtual_path);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &SelectFileFunction::GetLocalPathsOnFileThread));

  return true;
}

void SelectFileFunction::GetLocalPathsResponseOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(selected_files_.size(), static_cast<size_t>(1));

  int index;
  args_->GetInteger(1, &index);
  SelectFileDialog::Listener* listener = GetListener();
  if (listener) {
    listener->FileSelected(selected_files_[0], index, NULL);
  }
}

SelectFilesFunction::SelectFilesFunction() {
}

SelectFilesFunction::~SelectFilesFunction() {
}

bool SelectFilesFunction::RunImpl() {
  DCHECK_EQ(static_cast<size_t>(1), args_->GetSize());

  ListValue* path_list = NULL;
  args_->GetList(0, &path_list);
  DCHECK(path_list);

  std::string virtual_path;
  size_t len = path_list->GetSize();
  virtual_paths_.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    path_list->GetString(i, &virtual_path);
    virtual_paths_.push_back(virtual_path);
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &SelectFilesFunction::GetLocalPathsOnFileThread));

  return true;
}

void SelectFilesFunction::GetLocalPathsResponseOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SelectFileDialog::Listener* listener = GetListener();
  if (listener) {
    listener->MultiFilesSelected(selected_files_, NULL);
  }
}

bool CancelFileDialogFunction::RunImpl() {
  SelectFileDialog::Listener* listener = GetListener();
  if (listener) {
    listener->FileSelectionCanceled(NULL);
  }

  return true;
}

bool FileDialogStringsFunction::RunImpl() {
  result_.reset(new DictionaryValue());
  DictionaryValue* dict = reinterpret_cast<DictionaryValue*>(result_.get());

  dict->SetString("LOCALE_DATE_SHORT",
        l10n_util::GetStringUTF16(IDS_LOCALE_DATE_SHORT));
  dict->SetString("LOCALE_MONTHS_SHORT",
        l10n_util::GetStringUTF16(IDS_LOCALE_MONTHS_SHORT));
  dict->SetString("LOCALE_DAYS_SHORT",
        l10n_util::GetStringUTF16(IDS_LOCALE_DAYS_SHORT));
  dict->SetString("FILES_DISPLAYED_SUMMARY",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_FILES_DISPLAYED_SUMMARY));
  dict->SetString("FILES_SELECTED_SUMMARY",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_FILES_SELECTED_SUMMARY));

  dict->SetString("BODY_FONT_FAMILY",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_BODY_FONT_FAMILY));
  dict->SetString("BODY_FONT_SIZE",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_BODY_FONT_SIZE));

  dict->SetString("ROOT_DIRECTORY_LABEL",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_ROOT_DIRECTORY_LABEL));
  dict->SetString("NAME_COLUMN_LABEL",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_NAME_COLUMN_LABEL));
  dict->SetString("SIZE_COLUMN_LABEL",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SIZE_COLUMN_LABEL));
  dict->SetString("DATE_COLUMN_LABEL",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_DATE_COLUMN_LABEL));
  dict->SetString("PREVIEW_COLUMN_LABEL",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_PREVIEW_COLUMN_LABEL));

  dict->SetString("FILENAME_LABEL",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_FILENAME_LABEL));

  dict->SetString("CANCEL_LABEL",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_CANCEL_LABEL));
  dict->SetString("OPEN_LABEL",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_OPEN_LABEL));
  dict->SetString("SAVE_LABEL",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SAVE_LABEL));

  dict->SetString("SELECT_FOLDER_TITLE",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SELECT_FOLDER_TITLE));
  dict->SetString("SELECT_OPEN_FILE_TITLE",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SELECT_OPEN_FILE_TITLE));
  dict->SetString("SELECT_OPEN_MULTI_FILE_TITLE",
        l10n_util::GetStringUTF16(
            IDS_FILE_BROWSER_SELECT_OPEN_MULTI_FILE_TITLE));
  dict->SetString("SELECT_SAVEAS_FILE_TITLE",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SELECT_SAVEAS_FILE_TITLE));

  dict->SetString("COMPUTING_SELECTION",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_COMPUTING_SELECTION));
  dict->SetString("NOTHING_SELECTED",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_NOTHING_SELECTED));
  dict->SetString("ONE_FILE_SELECTED",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_ONE_FILE_SELECTED));
  dict->SetString("MANY_FILES_SELECTED",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_MANY_FILES_SELECTED));

  SendResponse(true);
  return true;
}
