// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_QUOTA_FILE_IO_H_
#define CONTENT_RENDERER_PEPPER_QUOTA_FILE_IO_H_

#include <deque>

#include "base/callback_forward.h"
#include "base/files/file_util_proxy.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_file_info.h"
#include "url/gurl.h"
#include "webkit/common/quota/quota_types.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// This class is created per PPB_FileIO_Impl instance and provides
// write operations for quota-managed files (i.e. files of
// PP_FILESYSTEMTYPE_LOCAL{PERSISTENT,TEMPORARY}).
class QuotaFileIO {
 public:
  // For quota handlings for FileIO API. This will be owned by QuotaFileIO.
  class Delegate {
   public:
    virtual ~Delegate() {}
    typedef base::Callback<void (int64)> AvailableSpaceCallback;
    virtual void QueryAvailableSpace(
        const GURL& origin,
        quota::StorageType type,
        const AvailableSpaceCallback& callback) = 0;
    virtual void WillUpdateFile(const GURL& file_path) = 0;
    virtual void DidUpdateFile(const GURL& file_path, int64_t delta) = 0;
    // Returns a MessageLoopProxy instance associated with the message loop of
    // the file thread in this renderer.
    virtual scoped_refptr<base::MessageLoopProxy>
        GetFileThreadMessageLoopProxy() = 0;
  };

  typedef base::FileUtilProxy::WriteCallback WriteCallback;
  typedef base::FileUtilProxy::StatusCallback StatusCallback;

  CONTENT_EXPORT QuotaFileIO(Delegate* delegate,
                             base::PlatformFile file,
                             const GURL& path_url,
                             PP_FileSystemType type);
  CONTENT_EXPORT ~QuotaFileIO();

  // Performs write or setlength operation with quota checks.
  // Returns true when the operation is successfully dispatched.
  // |bytes_to_write| must be geater than zero.
  // |callback| will be dispatched when the operation completes.
  // Otherwise it returns false and |callback| will not be dispatched.
  // |callback| will not be dispatched either when this instance is
  // destroyed before the operation completes.
  // SetLength/WillSetLength cannot be called while there're any in-flight
  // operations.  For Write/WillWrite it is guaranteed that |callback| are
  // always dispatched in the same order as Write being called.
  CONTENT_EXPORT bool Write(int64_t offset,
                            const char* buffer,
                            int32_t bytes_to_write,
                            const WriteCallback& callback);
  CONTENT_EXPORT bool WillWrite(int64_t offset,
                                int32_t bytes_to_write,
                                const WriteCallback& callback);

  CONTENT_EXPORT bool SetLength(int64_t length,
                                const StatusCallback& callback);
  CONTENT_EXPORT bool WillSetLength(int64_t length,
                                    const StatusCallback& callback);

  Delegate* delegate() const { return delegate_.get(); }

 private:
  class PendingOperationBase;
  class WriteOperation;
  class SetLengthOperation;

  bool CheckIfExceedsQuota(int64_t new_file_size) const;
  void WillUpdate();
  void DidWrite(WriteOperation* op, int64_t written_offset_end);
  void DidSetLength(base::PlatformFileError error, int64_t new_file_size);

  bool RegisterOperationForQuotaChecks(PendingOperationBase* op);
  void DidQueryInfoForQuota(base::PlatformFileError error_code,
                            const base::PlatformFileInfo& file_info);
  void DidQueryAvailableSpace(int64_t avail_space);
  void DidQueryForQuotaCheck();

  scoped_ptr<Delegate> delegate_;

  // The file information associated to this instance.
  base::PlatformFile file_;
  GURL file_url_;
  quota::StorageType storage_type_;

  // Pending operations that are waiting quota checks and pending
  // callbacks that are to be fired after the operation;
  // we use two separate queues for them so that we can safely dequeue the
  // pending callbacks while enqueueing new operations. (This could
  // happen when callbacks are dispatched synchronously due to error etc.)
  std::deque<PendingOperationBase*> pending_operations_;
  std::deque<PendingOperationBase*> pending_callbacks_;

  // Valid only while there're pending quota checks.
  int64_t cached_file_size_;
  int64_t cached_available_space_;

  // Quota-related queries and errors occurred during in-flight quota checks.
  int outstanding_quota_queries_;
  int outstanding_errors_;

  // For parallel writes bookkeeping.
  int64_t max_written_offset_;
  int inflight_operations_;

  base::WeakPtrFactory<QuotaFileIO> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaFileIO);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_QUOTA_FILE_IO_H_
