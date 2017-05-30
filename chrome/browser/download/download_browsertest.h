// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_BROWSERTEST_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_BROWSERTEST_H_

#include <stddef.h>

#include "base/macros.h"
#include "content/public/test/download_test_observer.h"

// DownloadTestObserver subclass that observes a download until it transitions
// from IN_PROGRESS to another state, but only after StartObserving() is called.
class DownloadTestObserverNotInProgress : public content::DownloadTestObserver {
 public:
  DownloadTestObserverNotInProgress(content::DownloadManager* download_manager,
                                    size_t count);
  ~DownloadTestObserverNotInProgress() override;

  void StartObserving();

 private:
  bool IsDownloadInFinalState(content::DownloadItem* download) override;

  bool started_observing_;

  DISALLOW_COPY_AND_ASSIGN(DownloadTestObserverNotInProgress);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_BROWSERTEST_H_
