// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_route_controller.h"

#include <string>
#include <utility>

#include "base/run_loop.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;

namespace media_router {

class MockMediaController : public mojom::MediaController {
 public:
  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD1(SetMute, void(bool mute));
  MOCK_METHOD1(SetVolume, void(float volume));
  MOCK_METHOD1(Seek, void(base::TimeDelta time));
};

class MockMediaRouteControllerObserver : public MediaRouteController::Observer {
 public:
  MockMediaRouteControllerObserver(
      scoped_refptr<MediaRouteController> controller)
      : MediaRouteController::Observer(controller) {}

  MOCK_METHOD1(OnMediaStatusUpdated, void(const MediaStatus& status));
};

class MediaRouteControllerTest : public ::testing::Test {
 public:
  MediaRouteControllerTest() {}
  ~MediaRouteControllerTest() override {}

  void SetUp() override {
    mojom::MediaControllerPtr media_controller_ptr;
    mojom::MediaControllerRequest media_controller_request =
        mojo::MakeRequest(&media_controller_ptr);
    media_controller_binding_ =
        base::MakeUnique<mojo::Binding<mojom::MediaController>>(
            &mock_media_controller_, std::move(media_controller_request));

    observer_ = base::MakeUnique<MockMediaRouteControllerObserver>(
        new MediaRouteController("routeId", std::move(media_controller_ptr)));
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

  MockMediaController mock_media_controller_;
  std::unique_ptr<mojo::Binding<mojom::MediaController>>
      media_controller_binding_;
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

  EXPECT_CALL(*observer1, OnMediaStatusUpdated(status));
  EXPECT_CALL(*observer2, OnMediaStatusUpdated(status));
  // TODO(takumif): Use a mojom::MediaStatusObserverPtr bound to the controller.
  GetController()->OnMediaStatusUpdated(status);

  observer1.reset();
  auto observer3 = CreateObserver();

  EXPECT_CALL(*observer2, OnMediaStatusUpdated(status));
  EXPECT_CALL(*observer3, OnMediaStatusUpdated(status));
  // TODO(takumif): Use a mojom::MediaStatusObserverPtr bound to the controller.
  GetController()->OnMediaStatusUpdated(status);
}

}  // namespace media_router
