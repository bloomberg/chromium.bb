// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_METRICS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_METRICS_H_

namespace media_router {

// Where the user clicked to open the Media Router dialog.
enum MediaRouterDialogOpenOrigin {
  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync.
  TOOLBAR = 0,
  OVERFLOW_MENU = 1,
  CONTEXTUAL_MENU = 2,
  PAGE = 3,

  // NOTE: Add entries only immediately above this line.
  TOTAL_COUNT = 4
};

class MediaRouterMetrics {
 public:
  // Records where the user clicked to open the Media Router dialog.
  static void RecordMediaRouterDialogOrigin(
      MediaRouterDialogOpenOrigin origin);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_METRICS_H_
