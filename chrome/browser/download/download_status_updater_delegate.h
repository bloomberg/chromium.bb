// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STATUS_UPDATER_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STATUS_UPDATER_DELEGATE_H_

#include "base/basictypes.h"

class DownloadStatusUpdaterDelegate {
 public:
  // Returns true if the progress is known (i.e. we know the final size
  // of all downloads).
  virtual bool IsDownloadProgressKnown() = 0;

  // Returns the number of downloads that are in progress.
  virtual int64 GetInProgressDownloadCount() = 0;

  // Returns the amount of received data, in bytes.
  virtual int64 GetReceivedDownloadBytes() = 0;

  // Returns the final size of all downloads, in bytes.
  virtual int64 GetTotalDownloadBytes() = 0;

 protected:
  virtual ~DownloadStatusUpdaterDelegate() {}
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STATUS_UPDATER_DELEGATE_H_
