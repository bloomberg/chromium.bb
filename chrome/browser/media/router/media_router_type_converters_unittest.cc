// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_router_type_converters.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

TEST(MediaRouterTypeConvertersTest, ConvertMediaSink) {
  MediaSink expected_media_sink("sinkId1", "Sink 1", MediaSink::IconType::CAST);
  interfaces::MediaSinkPtr mojo_sink(interfaces::MediaSink::New());
  mojo_sink->sink_id = "sinkId1";
  mojo_sink->name = "Sink 1";
  mojo_sink->icon_type =
      media_router::interfaces::MediaSink::IconType::ICON_TYPE_CAST;

  MediaSink media_sink = mojo::TypeConverter<
      media_router::MediaSink,
      media_router::interfaces::MediaSinkPtr>::Convert(mojo_sink);

  // Convert MediaSink and back should result in identical object.
  EXPECT_EQ(expected_media_sink.name(), media_sink.name());
  EXPECT_EQ(expected_media_sink.id(), media_sink.id());
  EXPECT_EQ(expected_media_sink.icon_type(), media_sink.icon_type());
  EXPECT_EQ(expected_media_sink.icon_type(), media_sink.icon_type());
  EXPECT_TRUE(expected_media_sink.Equals(media_sink));
}

TEST(MediaRouterTypeConvertersTest, ConvertMediaSinkIconType) {
  // Convert from Mojo to Media Router.
  EXPECT_EQ(media_router::MediaSink::CAST,
      mojo::SinkIconTypeFromMojo(
          media_router::interfaces::MediaSink::IconType::ICON_TYPE_CAST));
  EXPECT_EQ(media_router::MediaSink::CAST_AUDIO,
      mojo::SinkIconTypeFromMojo(
          media_router::interfaces::MediaSink::IconType::ICON_TYPE_CAST_AUDIO));
  EXPECT_EQ(media_router::MediaSink::CAST_AUDIO_GROUP,
      mojo::SinkIconTypeFromMojo(
          media_router::interfaces::MediaSink::
              IconType::ICON_TYPE_CAST_AUDIO_GROUP));
  EXPECT_EQ(media_router::MediaSink::GENERIC,
      mojo::SinkIconTypeFromMojo(
          media_router::interfaces::MediaSink::IconType::ICON_TYPE_GENERIC));
  EXPECT_EQ(media_router::MediaSink::HANGOUT,
      mojo::SinkIconTypeFromMojo(
          media_router::interfaces::MediaSink::IconType::ICON_TYPE_HANGOUT));

  // Convert from Media Router to Mojo.
  EXPECT_EQ(media_router::interfaces::MediaSink::IconType::ICON_TYPE_CAST,
      mojo::SinkIconTypeToMojo(media_router::MediaSink::CAST));
  EXPECT_EQ(media_router::interfaces::MediaSink::IconType::ICON_TYPE_CAST_AUDIO,
      mojo::SinkIconTypeToMojo(media_router::MediaSink::CAST_AUDIO));
  EXPECT_EQ(media_router::interfaces::MediaSink::
      IconType::ICON_TYPE_CAST_AUDIO_GROUP,
      mojo::SinkIconTypeToMojo(media_router::MediaSink::CAST_AUDIO_GROUP));
  EXPECT_EQ(media_router::interfaces::MediaSink::IconType::ICON_TYPE_GENERIC,
      mojo::SinkIconTypeToMojo(media_router::MediaSink::GENERIC));
  EXPECT_EQ(media_router::interfaces::MediaSink::IconType::ICON_TYPE_HANGOUT,
      mojo::SinkIconTypeToMojo(media_router::MediaSink::HANGOUT));
}

TEST(MediaRouterTypeConvertersTest, ConvertMediaRoute) {
  MediaSource expected_source(MediaSourceForTab(123));
  MediaRoute expected_media_route("routeId1", expected_source, "sinkId",
                                  "Description", false, "cast_view.html", true);
  interfaces::MediaRoutePtr mojo_route(interfaces::MediaRoute::New());
  mojo_route->media_route_id = "routeId1";
  mojo_route->media_source = expected_source.id();
  mojo_route->media_sink_id = "sinkId";
  mojo_route->description = "Description";
  mojo_route->is_local = false;
  mojo_route->custom_controller_path = "cast_view.html";
  mojo_route->for_display = true;

  MediaRoute media_route = mojo_route.To<MediaRoute>();
  EXPECT_TRUE(expected_media_route.Equals(media_route));
  EXPECT_EQ(expected_media_route.media_sink_id(), media_route.media_sink_id());
  EXPECT_EQ(expected_media_route.description(), media_route.description());
  EXPECT_TRUE(
      expected_media_route.media_source().Equals(media_route.media_source()));
  EXPECT_EQ(expected_media_route.media_source().id(),
            media_route.media_source().id());
  EXPECT_EQ(expected_media_route.is_local(), media_route.is_local());
  EXPECT_EQ(expected_media_route.custom_controller_path(),
            media_route.custom_controller_path());
  EXPECT_EQ(expected_media_route.for_display(), media_route.for_display());
}

TEST(MediaRouterTypeConvertersTest, ConvertMediaRouteWithoutOptionalFields) {
  MediaRoute expected_media_route("routeId1", MediaSource(), "sinkId",
                                  "Description", false, "", false);
  interfaces::MediaRoutePtr mojo_route(interfaces::MediaRoute::New());
  // MediaRoute::media_source is omitted.
  mojo_route->media_route_id = "routeId1";
  mojo_route->media_sink_id = "sinkId";
  mojo_route->description = "Description";
  mojo_route->is_local = false;
  mojo_route->for_display = false;

  MediaRoute media_route = mojo_route.To<MediaRoute>();
  EXPECT_TRUE(expected_media_route.Equals(media_route));
}

TEST(MediaRouterTypeConvertersTest, ConvertIssue) {
  interfaces::IssuePtr mojoIssue;
  mojoIssue = interfaces::Issue::New();
  mojoIssue->title = "title";
  mojoIssue->message = "msg";
  mojoIssue->route_id = "routeId";
  mojoIssue->default_action =
      interfaces::Issue::ActionType::ACTION_TYPE_LEARN_MORE;
  mojoIssue->secondary_actions =
      mojo::Array<interfaces::Issue::ActionType>::New(1);
  mojoIssue->secondary_actions[0] =
      interfaces::Issue::ActionType::ACTION_TYPE_DISMISS;
  mojoIssue->severity = interfaces::Issue::Severity::SEVERITY_WARNING;
  mojoIssue->is_blocking = true;
  mojoIssue->help_url = "help_url";

  std::vector<IssueAction> secondary_actions;
  secondary_actions.push_back(IssueAction(IssueAction::TYPE_DISMISS));
  Issue expected_issue("title", "msg",
                       IssueAction(IssueAction::TYPE_LEARN_MORE),
                       secondary_actions, "routeId", Issue::WARNING, true,
                       "help_url");
  Issue converted_issue = mojo::TypeConverter<
      media_router::Issue,
      media_router::interfaces::IssuePtr>::Convert(mojoIssue);

  EXPECT_EQ(expected_issue.title(), converted_issue.title());
  EXPECT_EQ(expected_issue.message(), converted_issue.message());
  EXPECT_EQ(expected_issue.default_action().type(),
            converted_issue.default_action().type());
  ASSERT_EQ(expected_issue.secondary_actions().size(),
            converted_issue.secondary_actions().size());
  for (size_t i = 0; i < expected_issue.secondary_actions().size(); ++i) {
    EXPECT_EQ(expected_issue.secondary_actions()[i].type(),
              converted_issue.secondary_actions()[i].type());
  }
  EXPECT_EQ(expected_issue.route_id(), converted_issue.route_id());
  EXPECT_EQ(expected_issue.severity(), converted_issue.severity());
  EXPECT_EQ(expected_issue.is_blocking(), converted_issue.is_blocking());
  EXPECT_EQ(expected_issue.help_url(), converted_issue.help_url());

  // Ensure that the internal Issue objects are considered distinct
  // (they possess different IDs.)
  EXPECT_FALSE(converted_issue.Equals(expected_issue));
}

TEST(MediaRouterTypeConvertersTest, ConvertIssueWithoutOptionalFields) {
  interfaces::IssuePtr mojoIssue;
  mojoIssue = interfaces::Issue::New();
  mojoIssue->title = "title";
  mojoIssue->default_action =
      interfaces::Issue::ActionType::ACTION_TYPE_DISMISS;
  mojoIssue->severity = interfaces::Issue::Severity::SEVERITY_WARNING;
  mojoIssue->is_blocking = true;

  Issue expected_issue("title", "", IssueAction(IssueAction::TYPE_DISMISS),
                       std::vector<IssueAction>(), "", Issue::WARNING, true,
                       "");

  Issue converted_issue = mojo::TypeConverter<
      media_router::Issue,
      media_router::interfaces::IssuePtr>::Convert(mojoIssue);

  EXPECT_EQ(expected_issue.title(), converted_issue.title());
  EXPECT_EQ(expected_issue.default_action().type(),
            converted_issue.default_action().type());
  EXPECT_EQ(0u, converted_issue.secondary_actions().size());
  EXPECT_EQ(expected_issue.severity(), converted_issue.severity());
  EXPECT_EQ(expected_issue.is_blocking(), converted_issue.is_blocking());

  // Ensure that the internal Issue objects are considered distinct
  // (they possess different IDs.)
  EXPECT_FALSE(converted_issue.Equals(expected_issue));
}

}  // namespace media_router
