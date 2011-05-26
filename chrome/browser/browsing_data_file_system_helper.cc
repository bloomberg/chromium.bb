// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_file_system_helper.h"

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebSecurityOrigin;

namespace {

class BrowsingDataFileSystemHelperImpl : public BrowsingDataFileSystemHelper {
 public:
  explicit BrowsingDataFileSystemHelperImpl(Profile* profile);

  virtual void StartFetching(
      Callback1<const std::vector<FileSystemInfo>& >::Type* callback);
  virtual void CancelNotification();
  virtual void DeleteFileSystemOrigin(const GURL& origin);

 private:
  virtual ~BrowsingDataFileSystemHelperImpl();

  // Enumerates all filesystem files in the FILE thread.
  void FetchFileSystemInfoInFileThread();
  // Notifies the completion callback in the UI thread.
  void NotifyInUIThread();
  // Delete data for an origin on the FILE thread.
  void DeleteFileSystemOriginInFileThread(const GURL& origin);

  Profile* profile_;

  // This only mutates in the FILE thread.
  std::vector<FileSystemInfo> file_system_info_;

  // This only mutates on the UI thread.
  scoped_ptr<Callback1<const std::vector<FileSystemInfo>& >::Type >
      completion_callback_;

  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notified the callback in the UI thread.
  // This only mutates on the UI thread.
  bool is_fetching_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataFileSystemHelperImpl);
};

BrowsingDataFileSystemHelperImpl::BrowsingDataFileSystemHelperImpl(
    Profile* profile)
    : profile_(profile),
      is_fetching_(false) {
  DCHECK(profile_);
}

BrowsingDataFileSystemHelperImpl::~BrowsingDataFileSystemHelperImpl() {
}

void BrowsingDataFileSystemHelperImpl::StartFetching(
    Callback1<const std::vector<FileSystemInfo>& >::Type* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_fetching_);
  DCHECK(callback);
  is_fetching_ = true;
  completion_callback_.reset(callback);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this,
          &BrowsingDataFileSystemHelperImpl::
              FetchFileSystemInfoInFileThread));
}

void BrowsingDataFileSystemHelperImpl::CancelNotification() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  completion_callback_.reset(NULL);
}

void BrowsingDataFileSystemHelperImpl::DeleteFileSystemOrigin(
    const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
       NewRunnableMethod(
           this,
           &BrowsingDataFileSystemHelperImpl::
               DeleteFileSystemOriginInFileThread,
           origin));
}

void BrowsingDataFileSystemHelperImpl::FetchFileSystemInfoInFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<fileapi::SandboxMountPointProvider::OriginEnumerator>
      origin_enumerator(profile_->GetFileSystemContext()->path_manager()->
          sandbox_provider()->CreateOriginEnumerator());

  // We don't own this pointer; deleting it would be a bad idea.
  fileapi::FileSystemQuotaUtil* quota_util = profile_->
      GetFileSystemContext()->GetQuotaUtil(fileapi::kFileSystemTypeTemporary);

  GURL current;
  while (!(current = origin_enumerator->Next()).is_empty()) {
    if (current.SchemeIs(chrome::kExtensionScheme)) {
      // Extension state is not considered browsing data.
      continue;
    }
    int64 persistent_usage = quota_util->GetOriginUsageOnFileThread(current,
        fileapi::kFileSystemTypePersistent);
    int64 temporary_usage = quota_util->GetOriginUsageOnFileThread(current,
        fileapi::kFileSystemTypeTemporary);
    file_system_info_.push_back(
        FileSystemInfo(
            current,
            origin_enumerator->HasFileSystemType(
                fileapi::kFileSystemTypePersistent),
            origin_enumerator->HasFileSystemType(
                fileapi::kFileSystemTypeTemporary),
            persistent_usage,
            temporary_usage));
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &BrowsingDataFileSystemHelperImpl::NotifyInUIThread));
}

void BrowsingDataFileSystemHelperImpl::NotifyInUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(is_fetching_);
  // Note: completion_callback_ mutates only in the UI thread, so it's safe to
  // test it here.
  if (completion_callback_ != NULL) {
    completion_callback_->Run(file_system_info_);
    completion_callback_.reset();
  }
  is_fetching_ = false;
}

void BrowsingDataFileSystemHelperImpl::DeleteFileSystemOriginInFileThread(
    const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_refptr<fileapi::FileSystemContext>
      context(profile_->GetFileSystemContext());
  context->DeleteDataForOriginOnFileThread(origin);
}

}  // namespace

BrowsingDataFileSystemHelper::FileSystemInfo::FileSystemInfo(
    const GURL& origin,
    bool has_persistent,
    bool has_temporary,
    int64 usage_persistent,
    int64 usage_temporary)
    : origin(origin),
      has_persistent(has_persistent),
      has_temporary(has_temporary),
      usage_persistent(usage_persistent),
      usage_temporary(usage_temporary) {
}

BrowsingDataFileSystemHelper::FileSystemInfo::~FileSystemInfo() {}

// static
BrowsingDataFileSystemHelper* BrowsingDataFileSystemHelper::Create(
    Profile* profile) {
  return new BrowsingDataFileSystemHelperImpl(profile);
}

CannedBrowsingDataFileSystemHelper::
PendingFileSystemInfo::PendingFileSystemInfo() {
}

CannedBrowsingDataFileSystemHelper::
PendingFileSystemInfo::PendingFileSystemInfo(const GURL& origin,
                                             const fileapi::FileSystemType type,
                                             int64 size)
    : origin(origin),
      type(type),
      size(size) {
}

CannedBrowsingDataFileSystemHelper::
PendingFileSystemInfo::~PendingFileSystemInfo() {
}

CannedBrowsingDataFileSystemHelper::CannedBrowsingDataFileSystemHelper(
    Profile* profile)
    : profile_(profile),
      is_fetching_(false) {
  DCHECK(profile);
}

CannedBrowsingDataFileSystemHelper::~CannedBrowsingDataFileSystemHelper() {}

CannedBrowsingDataFileSystemHelper*
    CannedBrowsingDataFileSystemHelper::Clone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CannedBrowsingDataFileSystemHelper* clone =
      new CannedBrowsingDataFileSystemHelper(profile_);

  clone->pending_file_system_info_ = pending_file_system_info_;
  clone->file_system_info_ = file_system_info_;
  return clone;
}

void CannedBrowsingDataFileSystemHelper::AddFileSystem(
    const GURL& origin, const fileapi::FileSystemType type, const int64 size) {
  pending_file_system_info_.push_back(
      PendingFileSystemInfo(origin, type, size));
}

void CannedBrowsingDataFileSystemHelper::Reset() {
  file_system_info_.clear();
  pending_file_system_info_.clear();
}

bool CannedBrowsingDataFileSystemHelper::empty() const {
  return file_system_info_.empty() && pending_file_system_info_.empty();
}

void CannedBrowsingDataFileSystemHelper::StartFetching(
    Callback1<const std::vector<FileSystemInfo>& >::Type* callback) {
  DCHECK(!is_fetching_);
  DCHECK(callback);
  is_fetching_ = true;
  completion_callback_.reset(callback);

  for (std::vector<PendingFileSystemInfo>::const_iterator
       info = pending_file_system_info_.begin();
       info != pending_file_system_info_.end(); ++info) {
    bool duplicate = false;
    for (std::vector<FileSystemInfo>::iterator
             file_system = file_system_info_.begin();
         file_system != file_system_info_.end();
         ++file_system) {
      if (file_system->origin == info->origin) {
        if (info->type == fileapi::kFileSystemTypePersistent) {
          file_system->has_persistent = true;
          file_system->usage_persistent = info->size;
        } else {
          file_system->has_temporary = true;
          file_system->usage_temporary = info->size;
        }
        duplicate = true;
        break;
      }
    }
    if (duplicate)
      continue;

    file_system_info_.push_back(FileSystemInfo(
        info->origin,
        (info->type == fileapi::kFileSystemTypePersistent),
        (info->type == fileapi::kFileSystemTypeTemporary),
        (info->type == fileapi::kFileSystemTypePersistent) ? info->size : 0,
        (info->type == fileapi::kFileSystemTypeTemporary) ? info->size : 0));
  }
  pending_file_system_info_.clear();

  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &CannedBrowsingDataFileSystemHelper::Notify));
}

void CannedBrowsingDataFileSystemHelper::Notify() {
  DCHECK(is_fetching_);
  if (completion_callback_ != NULL) {
    completion_callback_->Run(file_system_info_);
    completion_callback_.reset();
  }
  is_fetching_ = false;
}
