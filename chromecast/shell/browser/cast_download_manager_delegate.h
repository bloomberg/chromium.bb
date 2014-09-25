// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHELL_BROWSER_CAST_DOWNLOAD_MANAGER_DELEGATE_H_
#define CHROMECAST_SHELL_BROWSER_CAST_DOWNLOAD_MANAGER_DELEGATE_H_

#include "base/macros.h"
#include "content/public/browser/download_manager_delegate.h"

namespace chromecast {
namespace shell {

class CastDownloadManagerDelegate : public content::DownloadManagerDelegate {
 public:
  CastDownloadManagerDelegate();
  virtual ~CastDownloadManagerDelegate();

  // content::DownloadManagerDelegate implementation:
  virtual void GetNextId(
      const content::DownloadIdCallback& callback) OVERRIDE;
  virtual bool DetermineDownloadTarget(
      content::DownloadItem* item,
      const content::DownloadTargetCallback& callback) OVERRIDE;
  virtual bool ShouldOpenFileBasedOnExtension(
      const base::FilePath& path) OVERRIDE;
  virtual bool ShouldCompleteDownload(
      content::DownloadItem* item,
      const base::Closure& complete_callback) OVERRIDE;
  virtual bool ShouldOpenDownload(
      content::DownloadItem* item,
      const content::DownloadOpenDelayedCallback& callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastDownloadManagerDelegate);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_SHELL_BROWSER_CAST_DOWNLOAD_MANAGER_DELEGATE_H_