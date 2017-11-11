// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MOCK_MOJO_MEDIA_ROUTER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MOCK_MOJO_MEDIA_ROUTER_H_

#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

class MockMojoMediaRouter : public MockMediaRouter, public mojom::MediaRouter {
 public:
  MockMojoMediaRouter();
  ~MockMojoMediaRouter() override;

  // mojom::MediaRouter overrides:
  void RegisterMediaRouteProvider(mojom::MediaRouteProvider::Id provider_id,
                                  mojom::MediaRouteProviderPtr provider_ptr,
                                  RegisterMediaRouteProviderCallback callback) {
    RegisterMediaRouteProviderInternal(provider_id, provider_ptr, callback);
  }
  MOCK_METHOD3(RegisterMediaRouteProviderInternal,
               void(mojom::MediaRouteProvider::Id provider_id,
                    mojom::MediaRouteProviderPtr& provider_ptr,
                    RegisterMediaRouteProviderCallback& callback));
  MOCK_METHOD1(OnIssue, void(const IssueInfo& issue));
  MOCK_METHOD4(OnSinksReceived,
               void(mojom::MediaRouteProvider::Id provider_id,
                    const std::string& media_source,
                    const std::vector<MediaSinkInternal>& internal_sinks,
                    const std::vector<url::Origin>& origins));
  MOCK_METHOD4(OnRoutesUpdated,
               void(mojom::MediaRouteProvider::Id provider_id,
                    const std::vector<MediaRoute>& routes,
                    const std::string& media_source,
                    const std::vector<std::string>& joinable_route_ids));
  MOCK_METHOD2(OnSinkAvailabilityUpdated,
               void(mojom::MediaRouteProvider::Id provider_id,
                    mojom::MediaRouter::SinkAvailability availability));
  MOCK_METHOD2(OnPresentationConnectionStateChanged,
               void(const std::string& route_id,
                    content::PresentationConnectionState state));
  MOCK_METHOD3(OnPresentationConnectionClosed,
               void(const std::string& route_id,
                    content::PresentationConnectionCloseReason reason,
                    const std::string& message));
  MOCK_METHOD2(OnRouteMessagesReceived,
               void(const std::string& route_id,
                    const std::vector<content::PresentationConnectionMessage>&
                        messages));
  void OnMediaRemoterCreated(
      int32_t tab_id,
      media::mojom::MirrorServiceRemoterPtr remoter,
      media::mojom::MirrorServiceRemotingSourceRequest source_request) {
    OnMediaRemoterCreatedInternal(tab_id, remoter, source_request);
  }
  MOCK_METHOD3(
      OnMediaRemoterCreatedInternal,
      void(int32_t tab_id,
           media::mojom::MirrorServiceRemoterPtr& remoter,
           media::mojom::MirrorServiceRemotingSourceRequest& source_request));
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MOCK_MOJO_MEDIA_ROUTER_H_
