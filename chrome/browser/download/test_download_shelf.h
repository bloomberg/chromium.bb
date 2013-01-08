// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_TEST_DOWNLOAD_SHELF_H_
#define CHROME_BROWSER_DOWNLOAD_TEST_DOWNLOAD_SHELF_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/download/download_shelf.h"

// An implementation of DownloadShelf for testing.
class TestDownloadShelf : public DownloadShelf {
 public:
  TestDownloadShelf();
  virtual ~TestDownloadShelf();

  // DownloadShelf:

  virtual bool IsShowing() const OVERRIDE;
  virtual bool IsClosing() const OVERRIDE;
  virtual Browser* browser() const OVERRIDE;

 protected:
  virtual void DoAddDownload(content::DownloadItem* download) OVERRIDE;
  virtual void DoShow() OVERRIDE;
  virtual void DoClose() OVERRIDE;

 private:
  bool is_showing_;

  DISALLOW_COPY_AND_ASSIGN(TestDownloadShelf);
};

#endif  // CHROME_BROWSER_DOWNLOAD_TEST_DOWNLOAD_SHELF_H_
