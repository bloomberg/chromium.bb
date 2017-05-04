// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLABLE_METRICS_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLABLE_METRICS_H_

// This enum backs a UMA histogram and must be treated as append-only.
enum class InstallabilityCheckStatus {
  NOT_STARTED,
  NOT_COMPLETED,
  IN_PROGRESS_NON_PROGRESSIVE_WEB_APP,
  IN_PROGRESS_PROGRESSIVE_WEB_APP,
  COMPLETE_NON_PROGRESSIVE_WEB_APP,
  COMPLETE_PROGRESSIVE_WEB_APP,
  COUNT,
};

class InstallableMetrics {
 public:
  static void RecordMenuOpenHistogram(InstallabilityCheckStatus status);
  static void RecordMenuItemAddToHomescreenHistogram(
      InstallabilityCheckStatus status);
};

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLABLE_METRICS_H_
