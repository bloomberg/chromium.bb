// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_file_system_proxy.h"

#include "base/bind.h"
#include "base/platform_file.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/fileapi/file_system_file_util_proxy.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

using base::MessageLoopProxy;
using content::BrowserThread;
using fileapi::FileSystemOperationInterface;

namespace {

const char kGDataRootDirectory[] = "gdata";
const char kFeedField[] = "feed";

}  // namespace

namespace gdata {

base::FileUtilProxy::Entry GDataFileToFileUtilProxyEntry(
    const GDataFileBase& file) {
  base::FileUtilProxy::Entry entry;
  entry.is_directory = file.file_info().is_directory;

  // TODO(zelidrag): Add file name modification logic to enforce uniquness of
  // file paths across this file system.
  entry.name = file.file_name();

  entry.size = file.file_info().size;
  entry.last_modified_time = file.file_info().last_modified;
  return entry;
}

// Base class for proxy reply delegates. Keeps the track of the calling
// thread to ensure its specializations will provide reply on it.
class FindFileDelegateReplyBase : public FindFileDelegate {
 public:
  FindFileDelegateReplyBase(
      GDataFileSystem* file_system,
      const FilePath& search_file_path,
      bool require_content)
      : file_system_(file_system),
        search_file_path_(search_file_path),
        require_content_(require_content) {
    BrowserThread::ID thread_id;
    CHECK(BrowserThread::GetCurrentThreadIdentifier(&thread_id));
    replay_message_proxy_ =
        BrowserThread::GetMessageLoopProxyForThread(thread_id);
  }

  virtual ~FindFileDelegateReplyBase() {}

  // chromeos::FindFileDelegate overrides.
  virtual FindFileTraversalCommand OnEnterDirectory(
      const FilePath& current_directory_path,
      GDataDirectory* current_dir) {
    return CheckAndRefreshContent(current_directory_path, current_dir) ?
               FIND_FILE_CONTINUES : FIND_FILE_TERMINATES;
  }

 protected:

  // Checks if the content of the |directory| under |directory_path| needs to be
  // refreshed. Returns true if directory content is fresh, otherwise it kicks
  // off content request request. After feed content content is received and
  // processed in GDataFileSystem::OnGetDocuments(), that function will also
  // restart the initiated FindFileByPath() request.
  bool CheckAndRefreshContent(const FilePath& directory_path,
                              GDataDirectory* directory) {
    GURL feed_url;
    if (directory->NeedsRefresh(&feed_url)) {
      // If content is stale/non-existing, first fetch the content of the
      // directory in order to traverse it further.
      file_system_->StartDirectoryRefresh(
          GDataFileSystem::FindFileParams(
              search_file_path_,
              require_content_,
              directory_path,
              feed_url,
              true,    /* is_initial_feed */
              this));
      return false;
    }
    return true;
  }

 protected:
  GDataFileSystem* file_system_;
  // Search file path.
  FilePath search_file_path_;
  // True if the final directory content is required.
  bool require_content_;
  scoped_refptr<MessageLoopProxy> replay_message_proxy_;
};

// GetFileInfoDelegate is used to handle results of proxy's content search
// during GetFileInfo call and route reply to the calling message loop.
class GetFileInfoDelegate : public FindFileDelegateReplyBase {
 public:
  GetFileInfoDelegate(
      GDataFileSystem* file_system,
      const FilePath& search_file_path,
      const FileSystemOperationInterface::GetMetadataCallback& callback)
      : FindFileDelegateReplyBase(file_system,
                                  search_file_path,
                                  false  /* require_content */),
        callback_(callback) {
  }

  // GDataFileSystemProxy::FindFileDelegate overrides.
  virtual void OnFileFound(GDataFile* file) OVERRIDE {
    DCHECK(file);
    Reply(base::PLATFORM_FILE_OK, file->file_info(), search_file_path_);
  }

  virtual void OnDirectoryFound(const FilePath& directory_path,
                                GDataDirectory* dir) OVERRIDE {
    DCHECK(dir);
    Reply(base::PLATFORM_FILE_OK, dir->file_info(), search_file_path_);
  }

  virtual void OnError(base::PlatformFileError error) OVERRIDE {
    Reply(error, base::PlatformFileInfo(), FilePath());
  }

 private:

  // Relays reply back to the callback on calling thread.
  void Reply(base::PlatformFileError result,
             const base::PlatformFileInfo& file_info,
             const FilePath& platform_path) {
    if (!callback_.is_null()) {
      replay_message_proxy_->PostTask(FROM_HERE,
          Bind(&GetFileInfoDelegate::ReplyOnCallingThread,
               this,
               result,
               file_info,
               platform_path));
    }
  }

  // Responds to callback.
  void ReplyOnCallingThread(
      base::PlatformFileError result,
      const base::PlatformFileInfo& file_info,
      const FilePath& platform_path) {
    if (!callback_.is_null())
      callback_.Run(result, file_info, platform_path);
  }


  FileSystemOperationInterface::GetMetadataCallback callback_;
};


// ReadDirectoryDelegate is used to handle results of proxy's content search
// during ReadDirectories call and route reply to the calling message loop.
class ReadDirectoryDelegate : public FindFileDelegateReplyBase {
 public:
  ReadDirectoryDelegate(
      GDataFileSystem* file_system,
      const FilePath& search_file_path,
      const FileSystemOperationInterface::ReadDirectoryCallback& callback)
      : FindFileDelegateReplyBase(file_system,
                                  search_file_path,
                                  true  /* require_content */),
        callback_(callback) {
  }

  // GDataFileSystemProxy::FindFileDelegate overrides.
  virtual void OnFileFound(GDataFile* file) OVERRIDE {
    // We are not looking for a file here at all.
    Reply(base::PLATFORM_FILE_ERROR_NOT_FOUND,
          std::vector<base::FileUtilProxy::Entry>(), false);
  }

  virtual void OnDirectoryFound(const FilePath& directory_path,
                                GDataDirectory* directory) OVERRIDE {
    DCHECK(directory);
    // Since we are reading directory content here, we need to make sure
    // that the directory found actually contains fresh content.
    if (!CheckAndRefreshContent(directory_path, directory))
      return;

    std::vector<base::FileUtilProxy::Entry> results;
    if (!directory || !directory->file_info().is_directory) {
      Reply(base::PLATFORM_FILE_ERROR_NOT_FOUND, results, false);
      return;
    }

    // Convert gdata files to something File API stack can understand.
    for (GDataFileCollection::const_iterator iter =
              directory->children().begin();
         iter != directory->children().end(); ++iter) {
       results.push_back(GDataFileToFileUtilProxyEntry(*(iter->second)));
    }

    GURL unused;
    Reply(base::PLATFORM_FILE_OK, results, directory->NeedsRefresh(&unused));
  }

  virtual void OnError(base::PlatformFileError error) OVERRIDE {
    Reply(error, std::vector<base::FileUtilProxy::Entry>(), false);
  }

 private:

  // Relays reply back to the callback on calling thread.
  void Reply(base::PlatformFileError result,
             const std::vector<base::FileUtilProxy::Entry>& file_list,
             bool has_more) {
    if (!callback_.is_null()) {
      replay_message_proxy_->PostTask(FROM_HERE,
          Bind(&ReadDirectoryDelegate::ReplyOnCallingThread,
               this,
               result,
               file_list,
               has_more));
    }
  }

  // Responds to callback.
  void ReplyOnCallingThread(
      base::PlatformFileError result,
      const std::vector<base::FileUtilProxy::Entry>& file_list,
      bool has_more) {
    if (!callback_.is_null())
      callback_.Run(result, file_list, has_more);
  }

  FileSystemOperationInterface::ReadDirectoryCallback callback_;
};

// GDataFileSystemProxy class implementation.

GDataFileSystemProxy::GDataFileSystemProxy(Profile* profile)
    : file_system_(GDataFileSystemFactory::GetForProfile(profile)) {
  // Should be created from the file browser extension API (AddMountFunction)
  // on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataFileSystemProxy::~GDataFileSystemProxy() {
  // Should be deleted from the CrosMountPointProvider on IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void GDataFileSystemProxy::GetFileInfo(const GURL& file_url,
    const FileSystemOperationInterface::GetMetadataCallback& callback) {
  // what platform you're on.
  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    scoped_refptr<GetFileInfoDelegate> delegate(
        new GetFileInfoDelegate(NULL, FilePath(), callback));
    delegate->OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  file_system_->FindFileByPath(
      file_path, new GetFileInfoDelegate(file_system_, file_path, callback));
}


void GDataFileSystemProxy::ReadDirectory(const GURL& file_url,
    const FileSystemOperationInterface::ReadDirectoryCallback& callback) {
  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    scoped_refptr<ReadDirectoryDelegate> delegate(
        new ReadDirectoryDelegate(NULL, FilePath(), callback));
    delegate->OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  file_system_->FindFileByPath(
      file_path, new ReadDirectoryDelegate(file_system_, file_path, callback));
}

void GDataFileSystemProxy::Remove(const GURL& file_url, bool recursive,
    const FileSystemOperationInterface::StatusCallback& callback) {
  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system_->Remove(file_path, recursive, callback);
}

void GDataFileSystemProxy::CreateDirectory(
    const GURL& file_url,
    bool exclusive,
    bool recursive,
    const FileSystemOperationInterface::StatusCallback& callback) {
  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system_->CreateDirectory(file_path, exclusive, recursive, callback);
}

// static.
bool GDataFileSystemProxy::ValidateUrl(const GURL& url, FilePath* file_path) {
  // what platform you're on.
  fileapi::FileSystemType type = fileapi::kFileSystemTypeUnknown;
  if (!fileapi::CrackFileSystemURL(url, NULL, &type, file_path) ||
      type != fileapi::kFileSystemTypeExternal) {
    return false;
  }
  return true;
}

}  // namespace gdata
