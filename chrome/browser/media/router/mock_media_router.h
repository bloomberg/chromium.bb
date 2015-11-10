// Copyright 2015 The Chromium Authors. All rights reserved.
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

// Media Router mock class. Used for testing purposes.
class MockMediaRouter : public MediaRouter {
 public:
  MockMediaRouter();
  virtual ~MockMediaRouter();

  MOCK_METHOD5(CreateRoute,
               void(const MediaSource::Id& source,
                    const MediaSink::Id& sink_id,
                    const GURL& origin,
                    content::WebContents* web_contents,
                    const std::vector<MediaRouteResponseCallback>& callbacks));
  MOCK_METHOD5(JoinRoute,
               void(const MediaSource::Id& source,
                    const std::string& presentation_id,
                    const GURL& origin,
                    content::WebContents* web_contents,
                    const std::vector<MediaRouteResponseCallback>& callbacks));
  MOCK_METHOD1(CloseRoute, void(const MediaRoute::Id& route_id));
  MOCK_METHOD3(SendRouteMessage,
               void(const MediaRoute::Id& route_id,
                    const std::string& message,
                    const SendRouteMessageCallback& callback));
  void SendRouteBinaryMessage(
      const MediaRoute::Id& route_id,
      scoped_ptr<std::vector<uint8>> data,
      const SendRouteMessageCallback& callback) override {
    SendRouteBinaryMessageInternal(route_id, data.get(), callback);
  }
  MOCK_METHOD3(SendRouteBinaryMessageInternal,
               void(const MediaRoute::Id& route_id,
                    std::vector<uint8>* data,
                    const SendRouteMessageCallback& callback));
  MOCK_METHOD1(AddIssue, void(const Issue& issue));
  MOCK_METHOD1(ClearIssue, void(const Issue::Id& issue_id));
  MOCK_METHOD1(OnPresentationSessionDetached,
               void(const MediaRoute::Id& route_id));
  MOCK_CONST_METHOD0(HasLocalRoute, bool());
  MOCK_METHOD1(RegisterIssuesObserver, void(IssuesObserver* observer));
  MOCK_METHOD1(UnregisterIssuesObserver, void(IssuesObserver* observer));
  MOCK_METHOD1(RegisterMediaSinksObserver, bool(MediaSinksObserver* observer));
  MOCK_METHOD1(UnregisterMediaSinksObserver,
               void(MediaSinksObserver* observer));
  MOCK_METHOD1(RegisterMediaRoutesObserver,
               void(MediaRoutesObserver* observer));
  MOCK_METHOD1(UnregisterMediaRoutesObserver,
               void(MediaRoutesObserver* observer));
  MOCK_METHOD1(RegisterPresentationSessionMessagesObserver,
               void(PresentationSessionMessagesObserver* observer));
  MOCK_METHOD1(UnregisterPresentationSessionMessagesObserver,
               void(PresentationSessionMessagesObserver* observer));
  MOCK_METHOD1(RegisterLocalMediaRoutesObserver,
               void(LocalMediaRoutesObserver* observer));
  MOCK_METHOD1(UnregisterLocalMediaRoutesObserver,
               void(LocalMediaRoutesObserver* observer));
  MOCK_METHOD1(RegisterPresentationConnectionStateObserver,
               void(PresentationConnectionStateObserver* observer));
  MOCK_METHOD1(UnregisterPresentationConnectionStateObserver,
               void(PresentationConnectionStateObserver* observer));
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOCK_MEDIA_ROUTER_H_
