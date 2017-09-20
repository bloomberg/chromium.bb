// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/extension_media_route_provider_proxy.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/router/event_page_request_manager.h"
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::Invoke;
using testing::Mock;
using testing::StrictMock;
using testing::WithArg;

namespace media_router {

namespace {

using MockRouteCallback =
    base::MockCallback<MockMediaRouteProvider::RouteCallback>;
using MockTerminateRouteCallback =
    base::MockCallback<mojom::MediaRouteProvider::TerminateRouteCallback>;
using MockBoolCallback = base::MockCallback<base::OnceCallback<void(bool)>>;
using MockSearchSinksCallback =
    base::MockCallback<base::OnceCallback<void(const std::string&)>>;

const char kDescription[] = "description";
const bool kIsIncognito = false;
const char kSource[] = "source1";
const char kRouteId[] = "routeId";
const char kSinkId[] = "sink";
const int kTabId = 1;
const char kPresentationId[] = "presentationId";
const char kOrigin[] = "http://origin/";
const int kTimeoutMillis = 5 * 1000;

}  // namespace

class ExtensionMediaRouteProviderProxyTest : public testing::Test {
 public:
  ExtensionMediaRouteProviderProxyTest() = default;
  ~ExtensionMediaRouteProviderProxyTest() override = default;

 protected:
  void SetUp() override {
    request_manager_ = static_cast<MockEventPageRequestManager*>(
        EventPageRequestManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_, &MockEventPageRequestManager::Create));
    ON_CALL(*request_manager_, RunOrDeferInternal(_, _))
        .WillByDefault(Invoke([](base::OnceClosure& request,
                                 MediaRouteProviderWakeReason wake_reason) {
          std::move(request).Run();
        }));

    provider_proxy_ = base::MakeUnique<ExtensionMediaRouteProviderProxy>(
        &profile_, mojo::MakeRequest(&provider_proxy_ptr_));
    RegisterMockMediaRouteProvider();
  }

  const MediaSource media_source_ = MediaSource(kSource);
  const MediaRoute route_ = MediaRoute(kRouteId,
                                       media_source_,
                                       kSinkId,
                                       kDescription,
                                       false,
                                       "",
                                       false);
  std::unique_ptr<ExtensionMediaRouteProviderProxy> provider_proxy_;
  mojom::MediaRouteProviderPtr provider_proxy_ptr_;
  StrictMock<MockMediaRouteProvider> mock_provider_;
  MockEventPageRequestManager* request_manager_ = nullptr;
  std::unique_ptr<mojo::Binding<mojom::MediaRouteProvider>> binding_;

 private:
  void RegisterMockMediaRouteProvider() {
    mock_provider_.SetRouteToReturn(route_);

    mojom::MediaRouteProviderPtr mock_provider_ptr;
    binding_ = base::MakeUnique<mojo::Binding<mojom::MediaRouteProvider>>(
        &mock_provider_, mojo::MakeRequest(&mock_provider_ptr));
    provider_proxy_->RegisterMediaRouteProvider(std::move(mock_provider_ptr));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMediaRouteProviderProxyTest);
};

TEST_F(ExtensionMediaRouteProviderProxyTest, CreateRoute) {
  EXPECT_CALL(
      mock_provider_,
      CreateRouteInternal(
          kSource, kSinkId, kPresentationId, url::Origin(GURL(kOrigin)), kTabId,
          base::TimeDelta::FromMilliseconds(kTimeoutMillis), kIsIncognito, _))
      .WillOnce(WithArg<7>(Invoke(
          &mock_provider_, &MockMediaRouteProvider::RouteRequestSuccess)));

  MockRouteCallback callback;
  provider_proxy_->CreateRoute(
      kSource, kSinkId, kPresentationId, url::Origin(GURL(kOrigin)), kTabId,
      base::TimeDelta::FromMilliseconds(kTimeoutMillis), kIsIncognito,
      base::BindOnce(&MockRouteCallback::Run, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, JoinRoute) {
  EXPECT_CALL(
      mock_provider_,
      JoinRouteInternal(
          kSource, kPresentationId, url::Origin(GURL(kOrigin)), kTabId,
          base::TimeDelta::FromMilliseconds(kTimeoutMillis), kIsIncognito, _))
      .WillOnce(WithArg<6>(Invoke(
          &mock_provider_, &MockMediaRouteProvider::RouteRequestSuccess)));

  MockRouteCallback callback;
  provider_proxy_->JoinRoute(
      kSource, kPresentationId, url::Origin(GURL(kOrigin)), kTabId,
      base::TimeDelta::FromMilliseconds(kTimeoutMillis), kIsIncognito,
      base::BindOnce(&MockRouteCallback::Run, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, ConnectRouteByRouteId) {
  EXPECT_CALL(
      mock_provider_,
      ConnectRouteByRouteIdInternal(
          kSource, kRouteId, kPresentationId, url::Origin(GURL(kOrigin)),
          kTabId, base::TimeDelta::FromMilliseconds(kTimeoutMillis),
          kIsIncognito, _))
      .WillOnce(WithArg<7>(Invoke(
          &mock_provider_, &MockMediaRouteProvider::RouteRequestSuccess)));

  MockRouteCallback callback;
  provider_proxy_->ConnectRouteByRouteId(
      kSource, kRouteId, kPresentationId, url::Origin(GURL(kOrigin)), kTabId,
      base::TimeDelta::FromMilliseconds(kTimeoutMillis), kIsIncognito,
      base::BindOnce(&MockRouteCallback::Run, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, TerminateRoute) {
  EXPECT_CALL(mock_provider_, TerminateRouteInternal(kRouteId, _))
      .WillOnce(WithArg<1>(Invoke(
          &mock_provider_, &MockMediaRouteProvider::TerminateRouteSuccess)));

  MockTerminateRouteCallback callback;
  provider_proxy_->TerminateRoute(
      kRouteId, base::BindOnce(&MockTerminateRouteCallback::Run,
                               base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, SendRouteMessage) {
  const std::string message = "message";
  EXPECT_CALL(mock_provider_, SendRouteMessageInternal(kRouteId, message, _))
      .WillOnce(WithArg<2>(Invoke(
          &mock_provider_, &MockMediaRouteProvider::SendRouteMessageSuccess)));

  MockBoolCallback callback;
  provider_proxy_->SendRouteMessage(
      kRouteId, message,
      base::BindOnce(&MockBoolCallback::Run, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, SendRouteBinaryMessage) {
  std::vector<uint8_t> data = {42};
  EXPECT_CALL(mock_provider_,
              SendRouteBinaryMessageInternal(kRouteId, ElementsAre(42), _))
      .WillOnce(WithArg<2>(
          Invoke(&mock_provider_,
                 &MockMediaRouteProvider::SendRouteBinaryMessageSuccess)));

  MockBoolCallback callback;
  provider_proxy_->SendRouteBinaryMessage(
      kRouteId, data,
      base::BindOnce(&MockBoolCallback::Run, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, StartAndStopObservingMediaSinks) {
  EXPECT_CALL(mock_provider_, StopObservingMediaSinks(kSource));
  provider_proxy_->StopObservingMediaSinks(kSource);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, StartAndStopObservingMediaRoutes) {
  EXPECT_CALL(mock_provider_, StopObservingMediaRoutes(kSource));
  provider_proxy_->StopObservingMediaRoutes(kSource);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, StartListeningForRouteMessages) {
  EXPECT_CALL(mock_provider_, StartListeningForRouteMessages(kSource));
  provider_proxy_->StartListeningForRouteMessages(kSource);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, StopListeningForRouteMessages) {
  EXPECT_CALL(mock_provider_, StopListeningForRouteMessages(kSource));
  provider_proxy_->StopListeningForRouteMessages(kSource);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, DetachRoute) {
  EXPECT_CALL(mock_provider_, DetachRoute(kRouteId));
  provider_proxy_->DetachRoute(kRouteId);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, EnableMdnsDiscovery) {
  EXPECT_CALL(mock_provider_, EnableMdnsDiscovery());
  provider_proxy_->EnableMdnsDiscovery();
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, UpdateMediaSinks) {
  EXPECT_CALL(mock_provider_, UpdateMediaSinks(kSource));
  provider_proxy_->UpdateMediaSinks(kSource);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, SearchSinks) {
  EXPECT_CALL(mock_provider_, SearchSinksInternal(kSinkId, kSource, _, _))
      .WillOnce(WithArg<3>(Invoke(
          &mock_provider_, &MockMediaRouteProvider::SearchSinksSuccess)));

  auto sink_search_criteria = mojom::SinkSearchCriteria::New();
  MockSearchSinksCallback callback;
  provider_proxy_->SearchSinks(kSinkId, kSource,
                               std::move(sink_search_criteria),
                               base::BindOnce(&MockSearchSinksCallback::Run,
                                              base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, ProvideSinks) {
  const std::string provider_name = "provider name";
  MediaSinkInternal sink;
  sink.set_sink_id(kSinkId);
  const std::vector<media_router::MediaSinkInternal> sinks = {sink};

  EXPECT_CALL(mock_provider_, ProvideSinks(provider_name, ElementsAre(sink)));
  provider_proxy_->ProvideSinks(provider_name, sinks);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, CreateMediaRouteController) {
  EXPECT_CALL(mock_provider_,
              CreateMediaRouteControllerInternal(kRouteId, _, _, _))
      .WillOnce(WithArg<3>(
          Invoke(&mock_provider_,
                 &MockMediaRouteProvider::CreateMediaRouteControllerSuccess)));

  mojom::MediaControllerPtr controller_ptr;
  mojom::MediaControllerRequest controller_request =
      mojo::MakeRequest(&controller_ptr);
  mojom::MediaStatusObserverPtr observer_ptr;
  mojom::MediaStatusObserverRequest observer_request =
      mojo::MakeRequest(&observer_ptr);

  MockBoolCallback callback;
  provider_proxy_->CreateMediaRouteController(
      kRouteId, std::move(controller_request), std::move(observer_ptr),
      base::BindOnce(&MockBoolCallback::Run, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionMediaRouteProviderProxyTest, NotifyRequestManagerOnError) {
  // Invalidating the Mojo pointer to the MRP held by the proxy should make it
  // notify request manager.
  EXPECT_CALL(*request_manager_, OnMojoConnectionError());
  binding_.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
