// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/download/base_file.h"
#include "chrome/browser/download/download_types.h"

struct DownloadCreateInfo;
class DownloadManager;
class ResourceDispatcherHost;

// These objects live exclusively on the download thread and handle the writing
// operations for one download. These objects live only for the duration that
// the download is 'in progress': once the download has been completed or
// cancelled, the DownloadFile is destroyed.
class DownloadFile : public BaseFile {
 public:
  DownloadFile(const DownloadCreateInfo* info,
               DownloadManager* download_manager);
  virtual ~DownloadFile();

  // Cancels the download request associated with this file.
  void CancelDownloadRequest(ResourceDispatcherHost* rdh);

  int id() const { return id_; }
  DownloadManager* GetDownloadManager();

  virtual std::string DebugString() const;

 private:
  // The unique identifier for this download, assigned at creation by
  // the DownloadFileManager for its internal record keeping.
  int id_;

  // IDs for looking up the tab we are associated with.
  int child_id_;

  // Handle for informing the ResourceDispatcherHost of a UI based cancel.
  int request_id_;

  // DownloadManager this download belongs to.
  scoped_refptr<DownloadManager> download_manager_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFile);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
