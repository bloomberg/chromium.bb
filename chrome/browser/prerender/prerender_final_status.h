// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_FINAL_STATUS_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_FINAL_STATUS_H_

namespace prerender {

// FinalStatus indicates whether |this| was used, or why it was cancelled.
// NOTE: New values need to be appended, since they are used in histograms.
enum FinalStatus {
  FINAL_STATUS_USED,
  FINAL_STATUS_TIMED_OUT,
  FINAL_STATUS_EVICTED,
  FINAL_STATUS_MANAGER_SHUTDOWN,
  FINAL_STATUS_CLOSED,
  FINAL_STATUS_CREATE_NEW_WINDOW,
  FINAL_STATUS_PROFILE_DESTROYED,
  FINAL_STATUS_APP_TERMINATING,
  FINAL_STATUS_JAVASCRIPT_ALERT,
  FINAL_STATUS_AUTH_NEEDED,
  FINAL_STATUS_HTTPS,
  FINAL_STATUS_MAX,
};

void RecordFinalStatus(FinalStatus final_status);

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_FINAL_STATUS_H_
