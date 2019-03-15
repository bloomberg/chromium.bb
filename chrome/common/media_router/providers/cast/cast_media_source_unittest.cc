// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_router/providers/cast/cast_media_source.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

TEST(CastMediaSourceTest, FromCastURL) {
  MediaSource::Id source_id(
      "cast:ABCDEFAB?capabilities=video_out,audio_out"
      "&broadcastNamespace=namespace"
      "&broadcastMessage=message"
      "&clientId=12345"
      "&launchTimeout=30000"
      "&autoJoinPolicy=tab_and_origin_scoped");
  std::unique_ptr<CastMediaSource> source =
      CastMediaSource::FromMediaSourceId(source_id);
  ASSERT_TRUE(source);
  EXPECT_EQ(source_id, source->source_id());
  ASSERT_EQ(1u, source->app_infos().size());
  const CastAppInfo& app_info = source->app_infos()[0];
  EXPECT_EQ("ABCDEFAB", app_info.app_id);
  EXPECT_EQ(cast_channel::CastDeviceCapability::VIDEO_OUT |
                cast_channel::CastDeviceCapability::AUDIO_OUT,
            app_info.required_capabilities);
  const auto& broadcast_request = source->broadcast_request();
  ASSERT_TRUE(broadcast_request);
  EXPECT_EQ("namespace", broadcast_request->broadcast_namespace);
  EXPECT_EQ("message", broadcast_request->message);
  EXPECT_EQ("12345", source->client_id());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(30000), source->launch_timeout());
  EXPECT_EQ(AutoJoinPolicy::kTabAndOriginScoped, source->auto_join_policy());
}

TEST(CastMediaSourceTest, FromLegacyCastURL) {
  MediaSource::Id source_id(
      "https://google.com/cast#__castAppId__=ABCDEFAB(video_out,audio_out)"
      "/__castBroadcastNamespace__=namespace"
      "/__castBroadcastMessage__=message"
      "/__castClientId__=12345"
      "/__castLaunchTimeout__=30000"
      "/__castAutoJoinPolicy__=origin_scoped");
  std::unique_ptr<CastMediaSource> source =
      CastMediaSource::FromMediaSourceId(source_id);
  ASSERT_TRUE(source);
  EXPECT_EQ(source_id, source->source_id());
  ASSERT_EQ(1u, source->app_infos().size());
  const CastAppInfo& app_info = source->app_infos()[0];
  EXPECT_EQ("ABCDEFAB", app_info.app_id);
  EXPECT_EQ(cast_channel::CastDeviceCapability::VIDEO_OUT |
                cast_channel::CastDeviceCapability::AUDIO_OUT,
            app_info.required_capabilities);
  const auto& broadcast_request = source->broadcast_request();
  ASSERT_TRUE(broadcast_request);
  EXPECT_EQ("namespace", broadcast_request->broadcast_namespace);
  EXPECT_EQ("message", broadcast_request->message);
  EXPECT_EQ("12345", source->client_id());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(30000), source->launch_timeout());
  EXPECT_EQ(AutoJoinPolicy::kOriginScoped, source->auto_join_policy());
}

TEST(CastMediaSourceTest, FromPresentationURL) {
  MediaSource::Id source_id("https://google.com");
  std::unique_ptr<CastMediaSource> source =
      CastMediaSource::FromMediaSourceId(source_id);
  ASSERT_TRUE(source);
  EXPECT_EQ(source_id, source->source_id());
  ASSERT_EQ(2u, source->app_infos().size());
  EXPECT_EQ(kCastStreamingAppId, source->app_infos()[0].app_id);
  EXPECT_EQ(kCastStreamingAudioAppId, source->app_infos()[1].app_id);
  EXPECT_TRUE(source->client_id().empty());
  EXPECT_EQ(kDefaultLaunchTimeout, source->launch_timeout());
  EXPECT_EQ(AutoJoinPolicy::kNone, source->auto_join_policy());
}

TEST(CastMediaSourceTest, FromMirroringURN) {
  MediaSource::Id source_id("urn:x-org.chromium.media:source:tab:5");
  std::unique_ptr<CastMediaSource> source =
      CastMediaSource::FromMediaSourceId(source_id);
  ASSERT_TRUE(source);
  EXPECT_EQ(source_id, source->source_id());
  ASSERT_EQ(2u, source->app_infos().size());
  EXPECT_EQ(kCastStreamingAppId, source->app_infos()[0].app_id);
  EXPECT_EQ(kCastStreamingAudioAppId, source->app_infos()[1].app_id);
  EXPECT_TRUE(source->client_id().empty());
  EXPECT_EQ(kDefaultLaunchTimeout, source->launch_timeout());
  EXPECT_EQ(AutoJoinPolicy::kNone, source->auto_join_policy());
}

TEST(CastMediaSourceTest, FromDesktopUrn) {
  MediaSource::Id source_id("urn:x-org.chromium.media:source:desktop");
  std::unique_ptr<CastMediaSource> source =
      CastMediaSource::FromMediaSourceId(source_id);
  ASSERT_TRUE(source);
  EXPECT_EQ(source_id, source->source_id());
  ASSERT_EQ(1u, source->app_infos().size());
  EXPECT_EQ(kCastStreamingAppId, source->app_infos()[0].app_id);
  EXPECT_TRUE(source->client_id().empty());
  EXPECT_EQ(kDefaultLaunchTimeout, source->launch_timeout());
  EXPECT_EQ(AutoJoinPolicy::kNone, source->auto_join_policy());
}

TEST(CastMediaSourceTest, FromInvalidSource) {
  EXPECT_FALSE(CastMediaSource::FromMediaSourceId("invalid:source"));
  EXPECT_FALSE(CastMediaSource::FromMediaSourceId("file:///foo.mp4"));
  EXPECT_FALSE(CastMediaSource::FromMediaSourceId(""));
  EXPECT_FALSE(CastMediaSource::FromMediaSourceId("cast:"));

  // Missing app ID.
  EXPECT_FALSE(CastMediaSource::FromMediaSourceId("cast:?param=foo"));
  EXPECT_FALSE(CastMediaSource::FromMediaSourceId(
      "https://google.com/cast#__castAppId__=/param=foo"));
}

}  // namespace media_router
