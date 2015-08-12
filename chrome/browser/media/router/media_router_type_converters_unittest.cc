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
  MediaSink expected_media_sink("sinkId1", "Sink 1", true);
  interfaces::MediaSinkPtr expected_mojo_sink(interfaces::MediaSink::New());
  expected_mojo_sink->sink_id = "sinkId1";
  expected_mojo_sink->name = "Sink 1";
  expected_mojo_sink->is_launching = true;

  interfaces::MediaSinkPtr mojo_sink =
      mojo::TypeConverter<interfaces::MediaSinkPtr, MediaSink>::Convert(
          expected_media_sink);

  EXPECT_EQ(expected_mojo_sink, mojo_sink);

  MediaSink media_sink = mojo::TypeConverter<
      media_router::MediaSink,
      media_router::interfaces::MediaSinkPtr>::Convert(mojo_sink);

  // Convert MediaSink and back should result in identical object.
  EXPECT_EQ(expected_media_sink.name(), media_sink.name());
  EXPECT_EQ(expected_media_sink.id(), media_sink.id());
  EXPECT_EQ(expected_media_sink.is_launching(), media_sink.is_launching());
  EXPECT_TRUE(expected_media_sink.Equals(media_sink));
}

TEST(MediaRouterTypeConvertersTest, ConvertMediaRoute) {
  MediaSource expected_source(MediaSourceForTab(123));
  MediaRoute expected_media_route("routeId1", expected_source,
                                  MediaSink("sinkId", "sinkName"),
                                  "Description", false, "cast_view.html");
  interfaces::MediaRoutePtr expected_mojo_route(interfaces::MediaRoute::New());
  expected_mojo_route->media_route_id = "routeId1";
  expected_mojo_route->media_source = expected_source.id();
  expected_mojo_route->media_sink = interfaces::MediaSink::New();
  expected_mojo_route->media_sink->sink_id = "sinkId";
  expected_mojo_route->media_sink->name = "sinkName";
  expected_mojo_route->description = "Description";
  expected_mojo_route->is_local = false;
  expected_mojo_route->custom_controller_path = "cast_view.html";

  interfaces::MediaRoutePtr mojo_route = mojo::TypeConverter<
      media_router::interfaces::MediaRoutePtr,
      media_router::MediaRoute>::Convert(expected_media_route);
  EXPECT_EQ(expected_mojo_route, mojo_route);

  MediaRoute media_route = mojo_route.To<MediaRoute>();
  EXPECT_TRUE(expected_media_route.Equals(media_route));
  EXPECT_EQ(expected_media_route.media_sink().name(),
            media_route.media_sink().name());
  EXPECT_EQ(expected_media_route.media_sink().id(),
            media_route.media_sink().id());
  EXPECT_EQ(expected_media_route.description(), media_route.description());
  EXPECT_TRUE(
      expected_media_route.media_source().Equals(media_route.media_source()));
  EXPECT_EQ(expected_media_route.media_source().id(),
            media_route.media_source().id());
  EXPECT_EQ(expected_media_route.is_local(), media_route.is_local());
  EXPECT_EQ(expected_media_route.custom_controller_path(),
            media_route.custom_controller_path());
}

TEST(MediaRouterTypeConvertersTest, ConvertMediaRouteWithoutOptionalFields) {
  MediaRoute expected_media_route("routeId1", MediaSource(),
                                  MediaSink("sinkId", "sinkName", false),
                                  "Description", false, "");
  interfaces::MediaRoutePtr expected_mojo_route(interfaces::MediaRoute::New());
  // MediaRoute::media_source is omitted.
  expected_mojo_route->media_route_id = "routeId1";
  expected_mojo_route->media_sink = interfaces::MediaSink::New();
  expected_mojo_route->media_sink->sink_id = "sinkId";
  expected_mojo_route->media_sink->name = "sinkName";
  expected_mojo_route->media_sink->is_launching = false;
  expected_mojo_route->description = "Description";
  expected_mojo_route->is_local = false;

  interfaces::MediaRoutePtr mojo_route = mojo::TypeConverter<
      media_router::interfaces::MediaRoutePtr,
      media_router::MediaRoute>::Convert(expected_media_route);
  EXPECT_EQ(expected_mojo_route, mojo_route);

  MediaRoute media_route = mojo_route.To<MediaRoute>();
  EXPECT_TRUE(expected_media_route.Equals(media_route));
  EXPECT_EQ("sinkName", media_route.media_sink().name());
}

TEST(MediaRouterTypeConvertersTest, ConvertIssue) {
  interfaces::IssuePtr mojoIssue;
  mojoIssue = interfaces::Issue::New();
  mojoIssue->title = "title";
  mojoIssue->message = "msg";
  mojoIssue->route_id = "routeId";
  mojoIssue->default_action = interfaces::Issue::ActionType::ACTION_TYPE_OK;
  mojoIssue->secondary_actions =
      mojo::Array<interfaces::Issue::ActionType>::New(2);
  mojoIssue->secondary_actions[0] =
      interfaces::Issue::ActionType::ACTION_TYPE_CANCEL;
  mojoIssue->secondary_actions[1] =
      interfaces::Issue::ActionType::ACTION_TYPE_DISMISS;
  mojoIssue->severity = interfaces::Issue::Severity::SEVERITY_WARNING;
  mojoIssue->is_blocking = true;
  mojoIssue->help_url = "help_url";

  std::vector<IssueAction> secondary_actions;
  secondary_actions.push_back(IssueAction(IssueAction::TYPE_CANCEL));
  secondary_actions.push_back(IssueAction(IssueAction::TYPE_DISMISS));
  Issue expected_issue("title", "msg", IssueAction(IssueAction::TYPE_OK),
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
  mojoIssue->default_action = interfaces::Issue::ActionType::ACTION_TYPE_OK;
  mojoIssue->severity = interfaces::Issue::Severity::SEVERITY_WARNING;
  mojoIssue->is_blocking = true;

  Issue expected_issue("title", "", IssueAction(IssueAction::TYPE_OK),
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
