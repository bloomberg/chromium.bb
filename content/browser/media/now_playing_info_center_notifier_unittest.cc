// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/now_playing_info_center_notifier.h"

#include <memory>
#include <utility>

#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/now_playing/mock_now_playing_info_center_delegate.h"

namespace content {

using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;

class NowPlayingInfoCenterNotifierTest : public testing::Test {
 public:
  NowPlayingInfoCenterNotifierTest() = default;
  ~NowPlayingInfoCenterNotifierTest() override = default;

  void SetUp() override {
    auto mock_now_playing_info_center_delegate =
        std::make_unique<now_playing::MockNowPlayingInfoCenterDelegate>();
    mock_now_playing_info_center_delegate_ =
        mock_now_playing_info_center_delegate.get();
    notifier_ = std::make_unique<NowPlayingInfoCenterNotifier>(
        /*connector=*/nullptr,
        std::move(mock_now_playing_info_center_delegate));
  }

 protected:
  void SimulatePlaying() {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->playback_state = MediaPlaybackState::kPlaying;
    notifier_->MediaSessionInfoChanged(std::move(session_info));
  }

  void SimulatePaused() {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->playback_state = MediaPlaybackState::kPaused;
    notifier_->MediaSessionInfoChanged(std::move(session_info));
  }

  void SimulateStopped() { notifier_->MediaSessionInfoChanged(nullptr); }

  NowPlayingInfoCenterNotifier& notifier() { return *notifier_; }
  now_playing::MockNowPlayingInfoCenterDelegate&
  mock_now_playing_info_center_delegate() {
    return *mock_now_playing_info_center_delegate_;
  }

 private:
  std::unique_ptr<NowPlayingInfoCenterNotifier> notifier_;
  now_playing::MockNowPlayingInfoCenterDelegate*
      mock_now_playing_info_center_delegate_;

  DISALLOW_COPY_AND_ASSIGN(NowPlayingInfoCenterNotifierTest);
};

TEST_F(NowPlayingInfoCenterNotifierTest, ProperlyUpdatesPlaybackState) {
  EXPECT_CALL(
      mock_now_playing_info_center_delegate(),
      SetPlaybackState(
          now_playing::NowPlayingInfoCenterDelegate::PlaybackState::kPlaying))
      .Times(3);
  EXPECT_CALL(
      mock_now_playing_info_center_delegate(),
      SetPlaybackState(
          now_playing::NowPlayingInfoCenterDelegate::PlaybackState::kPaused))
      .Times(2);
  EXPECT_CALL(
      mock_now_playing_info_center_delegate(),
      SetPlaybackState(
          now_playing::NowPlayingInfoCenterDelegate::PlaybackState::kStopped))
      .Times(1);

  SimulatePlaying();
  SimulatePaused();
  SimulatePlaying();
  SimulateStopped();
  SimulatePlaying();
  SimulatePaused();
}

}  // namespace content
