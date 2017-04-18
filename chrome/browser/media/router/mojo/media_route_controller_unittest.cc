// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_route_controller.h"

#include <string>
#include <utility>

#include "base/run_loop.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    mojom::MediaControllerPtr media_controller_ptr;
    mojom::MediaControllerRequest media_controller_request =
        mojo::MakeRequest(&media_controller_ptr);
    mock_media_controller_.Bind(std::move(media_controller_request));

    observer_ = base::MakeUnique<MockMediaRouteControllerObserver>(
        base::MakeShared<MediaRouteController>(
            kRouteId, std::move(media_controller_ptr), &router_));
  }

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

  MockMediaRouter router_;
  MockMediaController mock_media_controller_;
  std::unique_ptr<MockMediaRouteControllerObserver> observer_;

  content::TestBrowserThreadBundle test_thread_bundle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaRouteControllerTest);
};

TEST_F(MediaRouteControllerTest, ForwardControllerCommands) {
  const float volume = 0.5;
  const base::TimeDelta time = base::TimeDelta::FromSeconds(42);

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

TEST_F(MediaRouteControllerTest, DestroyControllerOnDisconnect) {
  // DetachRouteController() should be called when the connection to
  // |mock_media_controller_| is invalidated.
  EXPECT_CALL(router_, DetachRouteController(kRouteId, GetController().get()))
      .Times(1);
  mock_media_controller_.CloseBinding();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&router_));
}

TEST_F(MediaRouteControllerTest, DestroyControllerOnNoObservers) {
  auto observer1 = CreateObserver();
  auto observer2 = CreateObserver();
  // Get a pointer to the controller to use in EXPECT_CALL().
  MediaRouteController* controller = GetController().get();
  // Get rid of |observer_| and its reference to the controller.
  observer_.reset();

  EXPECT_CALL(router_, DetachRouteController(kRouteId, controller)).Times(0);
  observer1.reset();

  // DetachRouteController() should be called when the controller no longer
  // has any observers.
  EXPECT_CALL(router_, DetachRouteController(kRouteId, controller)).Times(1);
  observer2.reset();
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&router_));
}

}  // namespace media_router
