// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_IMPL_H_
#pragma once

#include "content/browser/download/download_file.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "content/browser/download/base_file.h"
#include "content/browser/download/download_request_handle.h"

class PowerSaveBlocker;

struct DownloadCreateInfo;

namespace content {
class DownloadManager;
}

class CONTENT_EXPORT DownloadFileImpl : virtual public content::DownloadFile {
 public:
  // Takes ownership of the object pointed to by |request_handle|.
  // |bound_net_log| will be used for logging the download file's events.
  DownloadFileImpl(const DownloadCreateInfo* info,
                   DownloadRequestHandleInterface* request_handle,
                   content::DownloadManager* download_manager,
                   bool calculate_hash,
                   scoped_ptr<PowerSaveBlocker> power_save_blocker,
                   const net::BoundNetLog& bound_net_log);
  virtual ~DownloadFileImpl();

  // DownloadFile functions.
  virtual net::Error Initialize() OVERRIDE;
  virtual net::Error AppendDataToFile(const char* data,
                                      size_t data_len) OVERRIDE;
  virtual net::Error Rename(const FilePath& full_path) OVERRIDE;
  virtual void Detach() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual void Finish() OVERRIDE;
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

 private:
  // Send updates on our progress.
  void SendUpdate();

  // The base file instance.
  BaseFile file_;

  // The unique identifier for this download, assigned at creation by
  // the DownloadFileManager for its internal record keeping.
  content::DownloadId id_;

  // Used to trigger progress updates.
  scoped_ptr<base::RepeatingTimer<DownloadFileImpl> > update_timer_;

  // The handle to the request information.  Used for operations outside the
  // download system, specifically canceling a download.
  scoped_ptr<DownloadRequestHandleInterface> request_handle_;

  // DownloadManager this download belongs to.
  scoped_refptr<content::DownloadManager> download_manager_;

  // RAII handle to keep the system from sleeping while we're downloading.
  scoped_ptr<PowerSaveBlocker> power_save_blocker_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileImpl);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_IMPL_H_
