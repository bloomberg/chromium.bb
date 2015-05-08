// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOCK_MEDIA_ROUTER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOCK_MEDIA_ROUTER_H_

#include <string>
#include <vector>

#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

// Mocked out MediaRouter used in tests.
class MockMediaRouter : public MediaRouter {
 public:
  MockMediaRouter();
  virtual ~MockMediaRouter();

  MOCK_METHOD3(RequestRoute,
               void(const MediaSourceId& source,
                    const MediaSinkId& sink_id,
                    const MediaRouteResponseCallback& callback));
  MOCK_METHOD1(CloseRoute, void(const MediaRouteId& route_id));
  MOCK_METHOD2(PostMessage,
               void(const MediaRouteId& route_id, const std::string& message));

  MOCK_METHOD1(RegisterMediaSinksObserver, bool(MediaSinksObserver* observer));
  MOCK_METHOD1(UnregisterMediaSinksObserver,
               void(MediaSinksObserver* observer));
  MOCK_METHOD1(RegisterMediaRoutesObserver,
               bool(MediaRoutesObserver* observer));
  MOCK_METHOD1(UnregisterMediaRoutesObserver,
               void(MediaRoutesObserver* observer));
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOCK_MEDIA_ROUTER_H_
