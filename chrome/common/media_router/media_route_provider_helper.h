// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_ROUTER_MEDIA_ROUTE_PROVIDER_HELPER_H_
#define CHROME_COMMON_MEDIA_ROUTER_MEDIA_ROUTE_PROVIDER_HELPER_H_

namespace media_router {

// Each MediaRouteProvider is associated with a unique ID. This enum must be
// kept in sync with mojom::MediaRouteProvider::Id, except for |UNKNWON|, which
// is not present in the Mojo enum.
enum MediaRouteProviderId {
  EXTENSION,
  WIRED_DISPLAY,
  UNKNOWN  // New values must be added above this value.
};

}  // namespace media_router

#endif  // CHROME_COMMON_MEDIA_ROUTER_MEDIA_ROUTE_PROVIDER_HELPER_H_
