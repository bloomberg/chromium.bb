// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_TEST_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_TEST_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"
#include "chrome/browser/media/router/test_helper.h"
#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/event_page_tracker.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class MediaRouterMojoImpl;

class MockMediaRouteProvider : public mojom::MediaRouteProvider {
 public:
  MockMediaRouteProvider();
  ~MockMediaRouteProvider() override;

  MOCK_METHOD8(CreateRoute,
               void(const std::string& source_urn,
                    const std::string& sink_id,
                    const std::string& presentation_id,
                    const url::Origin& origin,
                    int tab_id,
                    base::TimeDelta timeout,
                    bool incognito,
                    const CreateRouteCallback& callback));
  MOCK_METHOD7(JoinRoute,
               void(const std::string& source_urn,
                    const std::string& presentation_id,
                    const url::Origin& origin,
                    int tab_id,
                    base::TimeDelta timeout,
                    bool incognito,
                    const JoinRouteCallback& callback));
  MOCK_METHOD8(ConnectRouteByRouteId,
               void(const std::string& source_urn,
                    const std::string& route_id,
                    const std::string& presentation_id,
                    const url::Origin& origin,
                    int tab_id,
                    base::TimeDelta timeout,
                    bool incognito,
                    const JoinRouteCallback& callback));
  MOCK_METHOD1(DetachRoute, void(const std::string& route_id));
  MOCK_METHOD2(TerminateRoute, void(const std::string& route_id,
                                    const TerminateRouteCallback& callback));
  MOCK_METHOD1(StartObservingMediaSinks, void(const std::string& source));
  MOCK_METHOD1(StopObservingMediaSinks, void(const std::string& source));
  MOCK_METHOD3(SendRouteMessage,
               void(const std::string& media_route_id,
                    const std::string& message,
                    const SendRouteMessageCallback& callback));
  void SendRouteBinaryMessage(
      const std::string& media_route_id,
      const std::vector<uint8_t>& data,
      const SendRouteMessageCallback& callback) override {
    SendRouteBinaryMessageInternal(media_route_id, data, callback);
  }
  MOCK_METHOD3(SendRouteBinaryMessageInternal,
               void(const std::string& media_route_id,
                    const std::vector<uint8_t>& data,
                    const SendRouteMessageCallback& callback));
  MOCK_METHOD1(StartListeningForRouteMessages,
               void(const std::string& route_id));
  MOCK_METHOD1(StopListeningForRouteMessages,
               void(const std::string& route_id));
  MOCK_METHOD1(OnPresentationSessionDetached,
               void(const std::string& route_id));
  MOCK_METHOD1(StartObservingMediaRoutes, void(const std::string& source));
  MOCK_METHOD1(StopObservingMediaRoutes, void(const std::string& source));
  MOCK_METHOD0(EnableMdnsDiscovery, void());
  MOCK_METHOD1(UpdateMediaSinks, void(const std::string& source));
  void SearchSinks(
      const std::string& sink_id,
      const std::string& media_source,
      mojom::SinkSearchCriteriaPtr search_criteria,
      const SearchSinksCallback& callback) override {
    SearchSinksInternal(sink_id, media_source, search_criteria, callback);
  }
  MOCK_METHOD4(SearchSinksInternal,
               void(const std::string& sink_id,
                    const std::string& media_source,
                    mojom::SinkSearchCriteriaPtr& search_criteria,
                    const SearchSinksCallback& callback));
  MOCK_METHOD2(ProvideSinks,
               void(const std::string&, const std::vector<MediaSinkInternal>&));
  void CreateMediaRouteController(
      const std::string& route_id,
      mojom::MediaControllerRequest media_controller,
      mojom::MediaStatusObserverPtr observer,
      const CreateMediaRouteControllerCallback& callback) override {
    CreateMediaRouteControllerInternal(route_id, media_controller, observer,
                                       callback);
  }
  MOCK_METHOD4(CreateMediaRouteControllerInternal,
               void(const std::string& route_id,
                    mojom::MediaControllerRequest& media_controller,
                    mojom::MediaStatusObserverPtr& observer,
                    const CreateMediaRouteControllerCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaRouteProvider);
};

class MockEventPageTracker : public extensions::EventPageTracker {
 public:
  MockEventPageTracker();
  ~MockEventPageTracker();

  MOCK_METHOD1(IsEventPageSuspended, bool(const std::string& extension_id));
  MOCK_METHOD2(WakeEventPage,
               bool(const std::string& extension_id,
                    const base::Callback<void(bool)>& callback));
};

class MockMediaController : public mojom::MediaController {
 public:
  MockMediaController();
  ~MockMediaController();

  void Bind(mojom::MediaControllerRequest request);
  mojom::MediaControllerPtr BindInterfacePtr();
  void CloseBinding();

  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD1(SetMute, void(bool mute));
  MOCK_METHOD1(SetVolume, void(float volume));
  MOCK_METHOD1(Seek, void(base::TimeDelta time));

 private:
  mojo::Binding<mojom::MediaController> binding_;
};

class MockMediaRouteController : public MediaRouteController {
 public:
  MockMediaRouteController(const MediaRoute::Id& route_id,
                           mojom::MediaControllerPtr mojo_media_controller,
                           MediaRouter* media_router);

  MOCK_CONST_METHOD0(Play, void());
  MOCK_CONST_METHOD0(Pause, void());
  MOCK_CONST_METHOD1(Seek, void(base::TimeDelta time));
  MOCK_CONST_METHOD1(SetMute, void(bool mute));
  MOCK_CONST_METHOD1(SetVolume, void(float volume));

 protected:
  // The dtor is protected because MockMediaRouteController is ref-counted.
  ~MockMediaRouteController() override;
};

class MockMediaRouteControllerObserver : public MediaRouteController::Observer {
 public:
  MockMediaRouteControllerObserver(
      scoped_refptr<MediaRouteController> controller);
  ~MockMediaRouteControllerObserver() override;

  MOCK_METHOD1(OnMediaStatusUpdated, void(const MediaStatus& status));
  MOCK_METHOD0(OnControllerInvalidated, void());
};

// Mockable class for awaiting RegisterMediaRouteProvider callbacks.
class RegisterMediaRouteProviderHandler {
 public:
  RegisterMediaRouteProviderHandler();
  ~RegisterMediaRouteProviderHandler();

  // A wrapper function to deal with move only parameter |config|.
  void Invoke(const std::string& instance_id,
              mojom::MediaRouteProviderConfigPtr config) {
    InvokeInternal(instance_id, config.get());
  }

  MOCK_METHOD2(InvokeInternal,
               void(const std::string& instance_id,
                    mojom::MediaRouteProviderConfig* config));
};

// Tests the API call flow between the MediaRouterMojoImpl and the Media Router
// Mojo service in both directions.
class MediaRouterMojoTest : public ::testing::Test {
 public:
  MediaRouterMojoTest();
  ~MediaRouterMojoTest() override;

 protected:
  void SetUp() override;
  void TearDown() override;

  void ProcessEventLoop();

  void ConnectProviderManagerService();

  const std::string& extension_id() const { return extension_->id(); }

  MediaRouterMojoImpl* router() const { return mock_media_router_.get(); }

  // Mock objects.
  MockMediaRouteProvider mock_media_route_provider_;
  testing::NiceMock<MockEventPageTracker> mock_event_page_tracker_;

  // Mojo proxy object for |mock_media_router_|
  media_router::mojom::MediaRouterPtr media_router_proxy_;

  RegisterMediaRouteProviderHandler provide_handler_;

 private:
  content::TestBrowserThreadBundle test_thread_bundle_;
  scoped_refptr<extensions::Extension> extension_;
  std::unique_ptr<MediaRouterMojoImpl> mock_media_router_;
  std::unique_ptr<mojo::Binding<mojom::MediaRouteProvider>> binding_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterMojoTest);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_TEST_H_
