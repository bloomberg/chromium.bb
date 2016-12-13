// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/mojo/media_router_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

TEST(MediaRouterTypeConvertersTest, ConvertMediaSink) {
  MediaSink expected_media_sink("sinkId1", "Sink 1", MediaSink::IconType::CAST);
  expected_media_sink.set_description("description");
  expected_media_sink.set_domain("domain");

  mojom::MediaSinkPtr mojo_sink(mojom::MediaSink::New());
  mojo_sink->sink_id = "sinkId1";
  mojo_sink->name = "Sink 1";
  mojo_sink->description = std::string("description");
  mojo_sink->domain = std::string("domain");
  mojo_sink->icon_type = media_router::mojom::MediaSink::IconType::CAST;

  MediaSink media_sink = mojo::TypeConverter<
      media_router::MediaSink,
      media_router::mojom::MediaSinkPtr>::Convert(mojo_sink);

  // Convert MediaSink and back should result in identical object.
  EXPECT_EQ(expected_media_sink.name(), media_sink.name());
  EXPECT_EQ(expected_media_sink.id(), media_sink.id());
  EXPECT_FALSE(media_sink.description().empty());
  EXPECT_EQ(expected_media_sink.description(), media_sink.description());
  EXPECT_FALSE(media_sink.domain().empty());
  EXPECT_EQ(expected_media_sink.domain(), media_sink.domain());
  EXPECT_EQ(expected_media_sink.icon_type(), media_sink.icon_type());
  EXPECT_TRUE(expected_media_sink.Equals(media_sink));
}

TEST(MediaRouterTypeConvertersTest, ConvertMediaSinkIconType) {
  // Convert from Mojo to Media Router.
  EXPECT_EQ(media_router::MediaSink::CAST,
            mojo::SinkIconTypeFromMojo(
                media_router::mojom::MediaSink::IconType::CAST));
  EXPECT_EQ(media_router::MediaSink::CAST_AUDIO,
            mojo::SinkIconTypeFromMojo(
                media_router::mojom::MediaSink::IconType::CAST_AUDIO));
  EXPECT_EQ(
      media_router::MediaSink::CAST_AUDIO_GROUP,
      mojo::SinkIconTypeFromMojo(
          media_router::mojom::MediaSink::IconType::CAST_AUDIO_GROUP));
  EXPECT_EQ(media_router::MediaSink::GENERIC,
            mojo::SinkIconTypeFromMojo(
                media_router::mojom::MediaSink::IconType::GENERIC));
  EXPECT_EQ(media_router::MediaSink::HANGOUT,
            mojo::SinkIconTypeFromMojo(
                media_router::mojom::MediaSink::IconType::HANGOUT));

  // Convert from Media Router to Mojo.
  EXPECT_EQ(media_router::mojom::MediaSink::IconType::CAST,
            mojo::SinkIconTypeToMojo(media_router::MediaSink::CAST));
  EXPECT_EQ(media_router::mojom::MediaSink::IconType::CAST_AUDIO,
            mojo::SinkIconTypeToMojo(media_router::MediaSink::CAST_AUDIO));
  EXPECT_EQ(
      media_router::mojom::MediaSink::IconType::CAST_AUDIO_GROUP,
      mojo::SinkIconTypeToMojo(media_router::MediaSink::CAST_AUDIO_GROUP));
  EXPECT_EQ(media_router::mojom::MediaSink::IconType::GENERIC,
            mojo::SinkIconTypeToMojo(media_router::MediaSink::GENERIC));
  EXPECT_EQ(media_router::mojom::MediaSink::IconType::HANGOUT,
            mojo::SinkIconTypeToMojo(media_router::MediaSink::HANGOUT));
}

TEST(MediaRouterTypeConvertersTest, ConvertMediaRoute) {
  MediaSource expected_source(MediaSourceForTab(123));
  MediaRoute expected_media_route("routeId1", expected_source, "sinkId",
                                  "Description", false, "cast_view.html", true);
  expected_media_route.set_incognito(true);
  expected_media_route.set_offscreen_presentation(true);
  mojom::MediaRoutePtr mojo_route(mojom::MediaRoute::New());
  mojo_route->media_route_id = "routeId1";
  mojo_route->media_source = expected_source.id();
  mojo_route->media_sink_id = "sinkId";
  mojo_route->description = "Description";
  mojo_route->is_local = false;
  mojo_route->custom_controller_path = std::string("cast_view.html");
  mojo_route->for_display = true;
  mojo_route->is_incognito = true;
  mojo_route->is_offscreen_presentation = true;

  MediaRoute media_route = mojo_route.To<MediaRoute>();
  EXPECT_TRUE(expected_media_route.Equals(media_route));
  EXPECT_EQ(expected_media_route.media_sink_id(), media_route.media_sink_id());
  EXPECT_EQ(expected_media_route.description(), media_route.description());
  EXPECT_EQ(expected_media_route.media_source(), media_route.media_source());
  EXPECT_EQ(expected_media_route.media_source().id(),
            media_route.media_source().id());
  EXPECT_EQ(expected_media_route.is_local(), media_route.is_local());
  EXPECT_EQ(expected_media_route.custom_controller_path(),
            media_route.custom_controller_path());
  EXPECT_EQ(expected_media_route.for_display(), media_route.for_display());
  EXPECT_EQ(expected_media_route.is_incognito(), media_route.is_incognito());
  EXPECT_EQ(expected_media_route.is_offscreen_presentation(),
            media_route.is_offscreen_presentation());
}

TEST(MediaRouterTypeConvertersTest, ConvertMediaRouteWithoutOptionalFields) {
  MediaRoute expected_media_route("routeId1", MediaSource("sourceId"), "sinkId",
                                  "Description", false, "", false);
  mojom::MediaRoutePtr mojo_route(mojom::MediaRoute::New());
  // MediaRoute::media_source is omitted.
  mojo_route->media_route_id = "routeId1";
  mojo_route->media_sink_id = "sinkId";
  mojo_route->description = "Description";
  mojo_route->is_local = false;
  mojo_route->for_display = false;
  mojo_route->is_incognito = false;

  MediaRoute media_route = mojo_route.To<MediaRoute>();
  EXPECT_TRUE(expected_media_route.Equals(media_route));
}

}  // namespace media_router
