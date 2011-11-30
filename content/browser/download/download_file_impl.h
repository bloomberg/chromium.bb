// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_IMPL_H_
#pragma once

#include "content/browser/download/download_file.h"

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/browser/download/download_request_handle.h"
#include "content/common/content_export.h"

struct DownloadCreateInfo;
class DownloadManager;

class CONTENT_EXPORT DownloadFileImpl : virtual public DownloadFile {
 public:
  // Takes ownership of the object pointed to by |request_handle|.
  DownloadFileImpl(const DownloadCreateInfo* info,
                   DownloadRequestHandleInterface* request_handle,
                   DownloadManager* download_manager);
  virtual ~DownloadFileImpl();

  // DownloadFile functions.
  virtual net::Error Initialize(bool calculate_hash) OVERRIDE;
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
  virtual bool GetSha256Hash(std::string* hash) OVERRIDE;
  virtual void CancelDownloadRequest() OVERRIDE;
  virtual int Id() const OVERRIDE;
  virtual DownloadManager* GetDownloadManager() OVERRIDE;
  virtual const DownloadId& GlobalId() const OVERRIDE;
  virtual std::string DebugString() const OVERRIDE;

 private:
  // The base file instance.
  BaseFile file_;

  // The unique identifier for this download, assigned at creation by
  // the DownloadFileManager for its internal record keeping.
  DownloadId id_;

  // The handle to the request information.  Used for operations outside the
  // download system, specifically canceling a download.
  scoped_ptr<DownloadRequestHandleInterface> request_handle_;

  // DownloadManager this download belongs to.
  scoped_refptr<DownloadManager> download_manager_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileImpl);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_IMPL_H_
