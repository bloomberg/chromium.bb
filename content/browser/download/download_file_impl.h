// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_IMPL_H_

#include "content/browser/download/download_file.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "content/browser/download/base_file.h"
#include "content/browser/download/byte_stream.h"
#include "content/browser/download/download_request_handle.h"
#include "net/base/net_log.h"

struct DownloadCreateInfo;

namespace content {
class ByteStreamReader;
class DownloadManager;
class PowerSaveBlocker;
}

class CONTENT_EXPORT DownloadFileImpl : virtual public content::DownloadFile {
 public:
  // Takes ownership of the object pointed to by |request_handle|.
  // |bound_net_log| will be used for logging the download file's events.
  DownloadFileImpl(const DownloadCreateInfo* info,
                   scoped_ptr<content::ByteStreamReader> stream,
                   DownloadRequestHandleInterface* request_handle,
                   scoped_refptr<content::DownloadManager> download_manager,
                   bool calculate_hash,
                   scoped_ptr<content::PowerSaveBlocker> power_save_blocker,
                   const net::BoundNetLog& bound_net_log);
  virtual ~DownloadFileImpl();

  // DownloadFile functions.
  virtual content::DownloadInterruptReason Initialize() OVERRIDE;
  virtual void Rename(const FilePath& full_path,
                      bool overwrite_existing_file,
                      const RenameCompletionCallback& callback) OVERRIDE;
  virtual void Detach() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual void AnnotateWithSourceInformation() OVERRIDE;
  virtual FilePath FullPath() const OVERRIDE;
  virtual bool InProgress() const OVERRIDE;
  virtual int64 BytesSoFar() const OVERRIDE;
  virtual int64 CurrentSpeed() const OVERRIDE;
  virtual bool GetHash(std::string* hash) OVERRIDE;
  virtual std::string GetHashState() OVERRIDE;
  virtual void CancelDownloadRequest() OVERRIDE;
  virtual int Id() const OVERRIDE;
  virtual content::DownloadManager* GetDownloadManager() OVERRIDE;
  virtual const content::DownloadId& GlobalId() const OVERRIDE;
  virtual std::string DebugString() const OVERRIDE;

 protected:
  // For test class overrides.
  virtual content::DownloadInterruptReason AppendDataToFile(
      const char* data, size_t data_len);

 private:
  // Send an update on our progress.
  void SendUpdate();

  // Called when there's some activity on stream_reader_ that needs to be
  // handled.
  void StreamActive();

  // The base file instance.
  BaseFile file_;

  // The stream through which data comes.
  // TODO(rdsmith): Move this into BaseFile; requires using the same
  // stream semantics in SavePackage.  Alternatively, replace SaveFile
  // with DownloadFile and get rid of BaseFile.
  scoped_ptr<content::ByteStreamReader> stream_reader_;

  // The unique identifier for this download, assigned at creation by
  // the DownloadFileManager for its internal record keeping.
  content::DownloadId id_;

  // The default directory for creating the download file.
  FilePath default_download_directory_;

  // Used to trigger progress updates.
  scoped_ptr<base::RepeatingTimer<DownloadFileImpl> > update_timer_;

  // The handle to the request information.  Used for operations outside the
  // download system, specifically canceling a download.
  scoped_ptr<DownloadRequestHandleInterface> request_handle_;

  // DownloadManager this download belongs to.
  scoped_refptr<content::DownloadManager> download_manager_;

  // Statistics
  size_t bytes_seen_;
  base::TimeDelta disk_writes_time_;
  base::TimeTicks download_start_;

  net::BoundNetLog bound_net_log_;

  base::WeakPtrFactory<DownloadFileImpl> weak_factory_;

  // RAII handle to keep the system from sleeping while we're downloading.
  scoped_ptr<content::PowerSaveBlocker> power_save_blocker_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileImpl);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_IMPL_H_
