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
#include "content/common/notification_service.h"
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

// GetFileSystemRootPathOnFileThread can only be called from the file thread,
// so here we are. This function takes a vector of virtual paths, converts
// them to local paths and calls GetLocalPathsResponseOnUIThread with the
// result vector, on the UI thread.
void FileDialogFunction::GetLocalPathsOnFileThread(
    const VirtualPathVec& virtual_paths) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePathVec local_paths;

  // FilePath(virtual_path) doesn't work on win, so limit this to ChromeOS.
#if defined(OS_CHROMEOS)
  GURL origin_url = source_url().GetOrigin();
  fileapi::FileSystemPathManager* path_manager =
      profile()->GetFileSystemContext()->path_manager();

  size_t len = virtual_paths.size();
  local_paths.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    std::string virtual_path = virtual_paths[i];
    FilePath root = path_manager->GetFileSystemRootPathOnFileThread(
        origin_url,
        fileapi::kFileSystemTypeLocal,
        FilePath(virtual_path),
        false);
    if (!root.empty()) {
      local_paths.push_back(root.Append(virtual_path));
    } else {
      LOG(WARNING) << "GetLocalPathsOnFileThread failure " << virtual_path;
    }
  }
#endif

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
          &FileDialogFunction::GetLocalPathsResponseOnUIThread,
          local_paths));
}

bool SelectFileFunction::RunImpl() {
  DCHECK_EQ(static_cast<size_t>(2), args_->GetSize());

  std::string virtual_path;
  args_->GetString(0, &virtual_path);
  args_->GetInteger(1, &index_);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &SelectFileFunction::GetLocalPathsOnFileThread,
          VirtualPathVec(1, virtual_path)));

  return true;
}

void SelectFileFunction::GetLocalPathsResponseOnUIThread(
    const FilePathVec& local_paths) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(local_paths.size(), static_cast<size_t>(1));

  if (!local_paths.empty())
    selected_file_ = local_paths[1];

  // Notify FileManagerDialog that a file has been selected.
  NotificationService::current()->Notify(
      NotificationType::FILE_BROWSE_SELECTED,
      Source<SelectFileFunction>(this),
      NotificationService::NoDetails());
}

bool SelectFilesFunction::RunImpl() {
  DCHECK_EQ(static_cast<size_t>(1), args_->GetSize());

  ListValue* path_list = NULL;
  args_->GetList(0, &path_list);
  DCHECK(path_list);

  size_t len = path_list->GetSize();
  std::string virtual_path;
  VirtualPathVec virtual_paths;
  virtual_paths.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    path_list->GetString(i, &virtual_path);
    virtual_paths.push_back(virtual_path);
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &SelectFilesFunction::GetLocalPathsOnFileThread,
          virtual_paths));

  return true;
}

void SelectFilesFunction::GetLocalPathsResponseOnUIThread(
    const FilePathVec& local_paths) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  selected_files_ = local_paths;

  // Notify FileManagerDialog that files have been selected.
  NotificationService::current()->Notify(
      NotificationType::FILE_BROWSE_MULTI_SELECTED,
      Source<SelectFilesFunction>(this),
      NotificationService::NoDetails());
}

bool CancelFileDialogFunction::RunImpl() {
  // Notify FileManagerDialog of cancellation.
  NotificationService::current()->Notify(
      NotificationType::FILE_BROWSE_CANCEL_DIALOG,
      Source<CancelFileDialogFunction>(this),
      NotificationService::NoDetails());

  return true;
}

bool FileDialogStringsFunction::RunImpl() {
  result_.reset(new DictionaryValue());
  DictionaryValue* dict = reinterpret_cast<DictionaryValue*>(result_.get());

  dict->SetString("DATE_ABBR",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_DATE_ABBR));
  dict->SetString("MONTH_ABBRS",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_MONTH_ABBRS));
  dict->SetString("DAY_ABBRS",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_DAY_ABBRS));
  dict->SetString("SHORT_DATE_FORMAT",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHORT_DATE_FORMAT));
  dict->SetString("FILES_DISPLAYED_SUMMARY",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_FILES_DISPLAYED_SUMMARY));
  dict->SetString("FILES_SELECTED_SUMMARY",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_FILES_SELECTED_SUMMARY));
  dict->SetString("FILE_IS_DIRECTORY",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_FILE_IS_DIRECTORY));
  dict->SetString("PARENT_DIRECTORY",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_PARENT_DIRECTORY));

  dict->SetString("ROOT_DIRECTORY_LABEL",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_ROOT_DIRECTORY_LABEL));
  dict->SetString("NAME_COLUMN_LABEL",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_NAME_COLUMN_LABEL));
  dict->SetString("SIZE_COLUMN_LABEL",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SIZE_COLUMN_LABEL));
  dict->SetString("DATE_COLUMN_LABEL",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_DATE_COLUMN_LABEL));

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
  dict->SetString("SELECT_SAVEAS_FILE",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SELECT_SAVEAS_FILE));

  dict->SetString("COMPUTING_SELECTION",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_COMPUTING_SELECTION));
  dict->SetString("NOTHING_SELECTED",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_NOTHING_SELECTED));
  dict->SetString("ONE_FILE_SELECTED",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_ONE_FILE_SELECTED));
  dict->SetString("MANY_FILES_SELECTED",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_MANY_FILES_SELECTED));

  dict->SetString("FILE_TYPE_UNKNOWN",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_FILE_TYPE_UNKNOWN));
  dict->SetString("FILE_TYPE_UNKNOWNS",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_FILE_TYPE_UNKNOWNS));
  dict->SetString("FILE_TYPE_FOLDER",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_FILE_TYPE_FOLDER));
  dict->SetString("FILE_TYPE_FOLDERS",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_FILE_TYPE_FOLDERS));
  dict->SetString("FILE_TYPE_IMAGE",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_FILE_TYPE_IMAGE));
  dict->SetString("FILE_TYPE_IMAGES",
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_FILE_TYPE_IMAGES));


  SendResponse(true);
  return true;
}

