// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/mac/now_playing_info_center_delegate.h"

#import <MediaPlayer/MediaPlayer.h>

#include "components/system_media_controls/mac/now_playing_info_center_delegate_cocoa.h"

namespace system_media_controls {
namespace internal {

namespace {

API_AVAILABLE(macos(10.12.2))
MPNowPlayingPlaybackState PlaybackStatusToMPNowPlayingPlaybackState(
    SystemMediaControls::PlaybackStatus status) {
  switch (status) {
    case SystemMediaControls::PlaybackStatus::kPlaying:
      return MPNowPlayingPlaybackStatePlaying;
    case SystemMediaControls::PlaybackStatus::kPaused:
      return MPNowPlayingPlaybackStatePaused;
    case SystemMediaControls::PlaybackStatus::kStopped:
      return MPNowPlayingPlaybackStateStopped;
    default:
      NOTREACHED();
  }
  return MPNowPlayingPlaybackStateUnknown;
}

}  // anonymous namespace

NowPlayingInfoCenterDelegate::NowPlayingInfoCenterDelegate() {
  now_playing_info_center_delegate_cocoa_.reset(
      [[NowPlayingInfoCenterDelegateCocoa alloc] init]);
}

NowPlayingInfoCenterDelegate::~NowPlayingInfoCenterDelegate() {
  [now_playing_info_center_delegate_cocoa_ resetNowPlayingInfo];
}

void NowPlayingInfoCenterDelegate::SetPlaybackStatus(
    SystemMediaControls::PlaybackStatus status) {
  MPNowPlayingPlaybackState state =
      PlaybackStatusToMPNowPlayingPlaybackState(status);
  [now_playing_info_center_delegate_cocoa_ setPlaybackState:state];
}

}  // namespace internal
}  // namespace system_media_controls
