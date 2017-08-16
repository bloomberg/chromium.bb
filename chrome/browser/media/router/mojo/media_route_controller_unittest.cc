// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_route_controller.h"

#include <string>
#include <utility>

#include "base/run_loop.h"
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::StrictMock;

namespace media_router {

namespace {

constexpr char kRouteId[] = "routeId";

}  // namespace

class MediaRouteControllerTest : public ::testing::Test {
 public:
  MediaRouteControllerTest() {}
  ~MediaRouteControllerTest() override {}

  void SetUp() override {
    SetUpMockObjects();

    auto controller =
        base::MakeRefCounted<MediaRouteController>(kRouteId, &profile_);
    mock_media_controller_.Bind(controller->CreateControllerRequest());
    observer_ = base::MakeUnique<MockMediaRouteControllerObserver>(
        std::move(controller));
  }

  void TearDown() override { observer_.reset(); }

  scoped_refptr<MediaRouteController> GetController() const {
    return observer_->controller();
  }

 protected:
  // This must be called only after |observer_| is set.
  std::unique_ptr<StrictMock<MockMediaRouteControllerObserver>> CreateObserver()
      const {
    return base::MakeUnique<StrictMock<MockMediaRouteControllerObserver>>(
        GetController());
  }

  content::TestBrowserThreadBundle test_thread_bundle_;
  TestingProfile profile_;

  MockMediaRouter* router_ = nullptr;
  MockEventPageRequestManager* request_manager_ = nullptr;
  MockMediaController mock_media_controller_;
  std::unique_ptr<MockMediaRouteControllerObserver> observer_;

 private:
  void SetUpMockObjects() {
    request_manager_ = static_cast<MockEventPageRequestManager*>(
        EventPageRequestManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_, &MockEventPageRequestManager::Create));
    request_manager_->set_mojo_connections_ready_for_test(true);

    router_ = static_cast<MockMediaRouter*>(
        MediaRouterFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_, &MockMediaRouter::Create));
  }

  DISALLOW_COPY_AND_ASSIGN(MediaRouteControllerTest);
};

// Test that when Mojo connections are ready, calls to the Mojo controller go
// through immediately.
TEST_F(MediaRouteControllerTest, ForwardControllerCommands) {
  const float volume = 0.5;
  const base::TimeDelta time = base::TimeDelta::FromSeconds(42);
  ASSERT_TRUE(request_manager_->mojo_connections_ready());

  EXPECT_CALL(mock_media_controller_, Play());
  GetController()->Play();
  EXPECT_CALL(mock_media_controller_, Pause());
  GetController()->Pause();
  EXPECT_CALL(mock_media_controller_, SetMute(true));
  GetController()->SetMute(true);
  EXPECT_CALL(mock_media_controller_, SetVolume(volume));
  GetController()->SetVolume(volume);
  EXPECT_CALL(mock_media_controller_, Seek(time));
  GetController()->Seek(time);

  base::RunLoop().RunUntilIdle();
}

// Tests that when Mojo connections aren't ready, calls to the Mojo controller
// get queued.
TEST_F(MediaRouteControllerTest, DoNotCallMojoControllerDirectly) {
  const float volume = 0.5;
  const base::TimeDelta time = base::TimeDelta::FromSeconds(42);
  std::vector<base::OnceClosure> requests;
  request_manager_->set_mojo_connections_ready_for_test(false);

  EXPECT_CALL(*request_manager_, RunOrDeferInternal(_, _))
      .WillRepeatedly(
          testing::WithArg<0>(Invoke([&requests](base::OnceClosure& request) {
            requests.push_back(std::move(request));
          })));
  // None of the calls to the Mojo controller should go through yet.
  EXPECT_CALL(mock_media_controller_, Play()).Times(0);
  EXPECT_CALL(mock_media_controller_, Pause()).Times(0);
  EXPECT_CALL(mock_media_controller_, SetMute(true)).Times(0);
  EXPECT_CALL(mock_media_controller_, SetVolume(volume)).Times(0);
  EXPECT_CALL(mock_media_controller_, Seek(time)).Times(0);
  GetController()->Play();
  GetController()->Pause();
  GetController()->SetMute(true);
  GetController()->SetVolume(volume);
  GetController()->Seek(time);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(request_manager_);
  testing::Mock::VerifyAndClearExpectations(&mock_media_controller_);

  // Execute all the queued requests. Now the calls to the Mojo controller
  // should go through.
  request_manager_->set_mojo_connections_ready_for_test(true);
  EXPECT_CALL(mock_media_controller_, Play());
  EXPECT_CALL(mock_media_controller_, Pause());
  EXPECT_CALL(mock_media_controller_, SetMute(true));
  EXPECT_CALL(mock_media_controller_, SetVolume(volume));
  EXPECT_CALL(mock_media_controller_, Seek(time));
  for (base::OnceClosure& request : requests)
    std::move(request).Run();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouteControllerTest, NotifyMediaRouteControllerObservers) {
  auto observer1 = CreateObserver();
  auto observer2 = CreateObserver();

  MediaStatus status;
  status.title = "test media status";

  // Get a mojo pointer for |controller_|, so that we can notify it of status
  // updates via mojo.
  mojom::MediaStatusObserverPtr mojo_observer =
      GetController()->BindObserverPtr();

  EXPECT_CALL(*observer1, OnMediaStatusUpdated(status));
  EXPECT_CALL(*observer2, OnMediaStatusUpdated(status));
  mojo_observer->OnMediaStatusUpdated(status);
  base::RunLoop().RunUntilIdle();

  observer1.reset();
  auto observer3 = CreateObserver();

  EXPECT_CALL(*observer2, OnMediaStatusUpdated(status));
  EXPECT_CALL(*observer3, OnMediaStatusUpdated(status));
  mojo_observer->OnMediaStatusUpdated(status);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouteControllerTest, DestroyControllerOnNoObservers) {
  auto observer1 = CreateObserver();
  auto observer2 = CreateObserver();
  // Get a pointer to the controller to use in EXPECT_CALL().
  MediaRouteController* controller = GetController().get();
  // Get rid of |observer_| and its reference to the controller.
  observer_.reset();

  EXPECT_CALL(*router_, DetachRouteController(kRouteId, controller)).Times(0);
  observer1.reset();

  // DetachRouteController() should be called when the controller no longer
  // has any observers.
  EXPECT_CALL(*router_, DetachRouteController(kRouteId, controller)).Times(1);
  observer2.reset();
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(router_));
}

}  // namespace media_router
