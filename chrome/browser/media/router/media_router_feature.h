// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FEATURE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FEATURE_H_

#include "base/feature_list.h"

namespace content {
class BrowserContext;
}

namespace media_router {

// Returns true if Media Router is enabled for |context|.
bool MediaRouterEnabled(content::BrowserContext* context);

#if !defined(OS_ANDROID)

extern const base::Feature kEnableDialSinkQuery;
extern const base::Feature kEnableCastDiscovery;
extern const base::Feature kCastMediaRouteProvider;
extern const base::Feature kEnableCastLocalMedia;

// Returns true if browser side DIAL sink query is enabled.
bool DialSinkQueryEnabled();

// Returns true if browser side Cast discovery is enabled.
bool CastDiscoveryEnabled();

// Returns true if browser side Cast Media Route Provider and sink query are
// enabled.
bool CastMediaRouteProviderEnabled();

// Returns true if local media casting is enabled.
bool CastLocalMediaEnabled();

// Returns true if the presentation receiver window for local media casting is
// available on the current platform.
// TODO(crbug.com/802332): Remove this when mac_views_browser=1 by default.
bool PresentationReceiverWindowEnabled();

#endif

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FEATURE_H_
