// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_RESUME_MODE_H_
#define CONTENT_BROWSER_DOWNLOAD_RESUME_MODE_H_

// The means by which the download was resumed.
// Used by DownloadItemImpl and UKM metrics.
namespace content {

enum class ResumeMode {
  INVALID = 0,
  IMMEDIATE_CONTINUE,
  IMMEDIATE_RESTART,
  USER_CONTINUE,
  USER_RESTART
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_RESUME_MODE_H_
