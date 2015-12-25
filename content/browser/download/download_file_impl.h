// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_IMPL_H_

#include "content/browser/download/download_file.h"

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/base_file.h"
#include "content/browser/download/rate_estimator.h"
#include "content/public/browser/download_save_info.h"
#include "net/log/net_log.h"

namespace content {
class ByteStreamReader;
class DownloadDestinationObserver;
class DownloadManager;
struct DownloadCreateInfo;

class CONTENT_EXPORT DownloadFileImpl : public DownloadFile {
 public:
  // Takes ownership of the object pointed to by |request_handle|.
  // |bound_net_log| will be used for logging the download file's events.
  // May be constructed on any thread.  All methods besides the constructor
  // (including destruction) must occur on the FILE thread.
  //
  // Note that the DownloadFileImpl automatically reads from the passed in
  // stream, and sends updates and status of those reads to the
  // DownloadDestinationObserver.
  DownloadFileImpl(
    scoped_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_downloads_directory,
    const GURL& url,
    const GURL& referrer_url,
    bool calculate_hash,
    scoped_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    base::WeakPtr<DownloadDestinationObserver> observer);

  ~DownloadFileImpl() override;

  // DownloadFile functions.
  void Initialize(const InitializeCallback& callback) override;
  void RenameAndUniquify(const base::FilePath& full_path,
                         const RenameCompletionCallback& callback) override;
  void RenameAndAnnotate(const base::FilePath& full_path,
                         const RenameCompletionCallback& callback) override;
  void Detach() override;
  void Cancel() override;
  base::FilePath FullPath() const override;
  bool InProgress() const override;
  int64_t CurrentSpeed() const override;
  bool GetHash(std::string* hash) override;
  std::string GetHashState() override;
  void SetClientGuid(const std::string& guid) override;

 protected:
  // For test class overrides.
  virtual DownloadInterruptReason AppendDataToFile(
      const char* data, size_t data_len);

  virtual base::TimeDelta GetRetryDelayForFailedRename(int attempt_number);

  virtual bool ShouldRetryFailedRename(DownloadInterruptReason reason);

 private:
  friend class DownloadFileTest;

  // Options for RenameWithRetryInternal.
  enum RenameOption {
    UNIQUIFY = 1 << 0,  // If there's already a file on disk that conflicts with
                        // |new_path|, try to create a unique file by appending
                        // a uniquifier.
    ANNOTATE_WITH_SOURCE_INFORMATION = 1 << 1
  };

  // Rename file_ to |new_path|.
  // |option| specifies additional operations to be performed during the rename.
  //     See RenameOption above.
  // |retries_left| indicates how many times to retry the operation if the
  //     rename fails with a transient error.
  // |time_of_first_failure| Set to an empty base::TimeTicks during the first
  //     call. Once the first failure is seen, subsequent calls of
  //     RenameWithRetryInternal will have a non-empty value keeping track of
  //     the time of first observed failure.  Used for UMA.
  void RenameWithRetryInternal(const base::FilePath& new_path,
                               RenameOption option,
                               int retries_left,
                               base::TimeTicks time_of_first_failure,
                               const RenameCompletionCallback& callback);

  // Send an update on our progress.
  void SendUpdate();

  // Called when there's some activity on stream_reader_ that needs to be
  // handled.
  void StreamActive();

  // The base file instance.
  BaseFile file_;

  // The default directory for creating the download file.
  base::FilePath default_download_directory_;

  // The stream through which data comes.
  // TODO(rdsmith): Move this into BaseFile; requires using the same
  // stream semantics in SavePackage.  Alternatively, replace SaveFile
  // with DownloadFile and get rid of BaseFile.
  scoped_ptr<ByteStreamReader> stream_reader_;

  // Used to trigger progress updates.
  scoped_ptr<base::RepeatingTimer> update_timer_;

  // Statistics
  size_t bytes_seen_;
  base::TimeDelta disk_writes_time_;
  base::TimeTicks download_start_;
  RateEstimator rate_estimator_;

  net::BoundNetLog bound_net_log_;

  base::WeakPtr<DownloadDestinationObserver> observer_;

  base::WeakPtrFactory<DownloadFileImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_IMPL_H_
