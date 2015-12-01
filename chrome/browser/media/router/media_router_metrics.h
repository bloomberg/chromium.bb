// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_METRICS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_METRICS_H_

namespace media_router {

// NOTE: Do not renumber enums as that would confuse interpretation of
// previously logged data. When making changes, also update the enum list
// in tools/metrics/histograms/histograms.xml to keep it in sync.

// Where the user clicked to open the Media Router dialog.
enum class MediaRouterDialogOpenOrigin {
  TOOLBAR = 0,
  OVERFLOW_MENU = 1,
  CONTEXTUAL_MENU = 2,
  PAGE = 3,

  // NOTE: Add entries only immediately above this line.
  TOTAL_COUNT = 4
};

// Why the Media Route Provider process was woken up.
enum class MediaRouteProviderWakeReason {
  CREATE_ROUTE = 0,
  JOIN_ROUTE = 1,
  CLOSE_ROUTE = 2,
  SEND_SESSION_MESSAGE = 3,
  SEND_SESSION_BINARY_MESSAGE = 4,
  PRESENTATION_SESSION_DETACHED = 5,
  START_OBSERVING_MEDIA_SINKS = 6,
  STOP_OBSERVING_MEDIA_SINKS = 7,
  START_OBSERVING_MEDIA_ROUTES = 8,
  STOP_OBSERVING_MEDIA_ROUTES = 9,
  LISTEN_FOR_ROUTE_MESSAGES = 10,
  STOP_LISTENING_FOR_ROUTE_MESSAGES = 11,
  CONNECTION_ERROR = 12,
  REGISTER_MEDIA_ROUTE_PROVIDER = 13,

  // NOTE: Add entries only immediately above this line.
  TOTAL_COUNT = 14
};

class MediaRouterMetrics {
 public:
  // Records where the user clicked to open the Media Router dialog.
  static void RecordMediaRouterDialogOrigin(
      MediaRouterDialogOpenOrigin origin);

  // Records why the media route provider extension was woken up.
  static void RecordMediaRouteProviderWakeReason(
      MediaRouteProviderWakeReason reason);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_METRICS_H_
