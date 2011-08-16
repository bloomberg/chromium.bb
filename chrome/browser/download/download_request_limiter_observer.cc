// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_limiter_observer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_request_limiter.h"

DownloadRequestLimiterObserver::DownloadRequestLimiterObserver(
    TabContents* tab_contents)
    : TabContentsObserver(tab_contents) {
}

DownloadRequestLimiterObserver::~DownloadRequestLimiterObserver() {
}

void DownloadRequestLimiterObserver::DidGetUserGesture() {
  if (!g_browser_process->download_request_limiter())
    return;  // NULL in unitests.
  g_browser_process->download_request_limiter()->OnUserGesture(tab_contents());
}
