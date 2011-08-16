// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_LIMITER_OBSERVER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_LIMITER_OBSERVER_H_

#include "content/browser/tab_contents/tab_contents_observer.h"

class TabContents;

// Watches for user gesture notifications.
class DownloadRequestLimiterObserver : public TabContentsObserver {
 public:
  explicit DownloadRequestLimiterObserver(TabContents* tab_contents);
  virtual ~DownloadRequestLimiterObserver();

  // TabContentsObserver overrides.
  virtual void DidGetUserGesture() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadRequestLimiterObserver);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_LIMITER_OBSERVER_H_
