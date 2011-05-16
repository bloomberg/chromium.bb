// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_TAB_HELPER_DELEGATE_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_TAB_HELPER_DELEGATE_H_
#pragma once

#include "base/basictypes.h"

class DownloadItem;
class TabContentsWrapper;

// Objects implement this interface to get notified about changes in the
// DownloadTabHelper and to provide necessary functionality.
class DownloadTabHelperDelegate {
 public:
  virtual bool CanDownload(int request_id) = 0;

  virtual void OnStartDownload(DownloadItem* download,
                               TabContentsWrapper* tab) = 0;

 protected:
  virtual ~DownloadTabHelperDelegate();
};

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_TAB_HELPER_DELEGATE_H_
