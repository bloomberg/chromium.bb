// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_webui_message_handler.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class TestingMediaRouterWebUIMessageHandler
    : public MediaRouterWebUIMessageHandler {
 public:
  explicit TestingMediaRouterWebUIMessageHandler(content::WebUI* web_ui) {
    set_web_ui(web_ui);
  }

  ~TestingMediaRouterWebUIMessageHandler() override { set_web_ui(nullptr); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingMediaRouterWebUIMessageHandler);
};

class MediaRouterWebUIMessageHandlerTest : public testing::Test {
 public:
  MediaRouterWebUIMessageHandlerTest() : web_ui_(), handler_(&web_ui_) {}
  ~MediaRouterWebUIMessageHandlerTest() override {}

 protected:
  content::TestWebUI web_ui_;
  TestingMediaRouterWebUIMessageHandler handler_;
};

TEST_F(MediaRouterWebUIMessageHandlerTest, UpdateSinks) {
  MediaSink::Id sink_id("sinkId123");
  std::string sink_name("The sink");
  std::vector<MediaSinkWithCastModes> media_sink_with_cast_modes_list;
  MediaSinkWithCastModes media_sink_with_cast_modes(
      MediaSink(sink_id, sink_name));
  media_sink_with_cast_modes.cast_modes.insert(MediaCastMode::TAB_MIRROR);
  media_sink_with_cast_modes_list.push_back(media_sink_with_cast_modes);

  handler_.UpdateSinks(media_sink_with_cast_modes_list);
  EXPECT_EQ(1u, web_ui_.call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_.call_data()[0];
  EXPECT_EQ("media_router.ui.setSinkList", call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::ListValue* sinks_list_value = nullptr;
  EXPECT_TRUE(arg1->GetAsList(&sinks_list_value));
  const base::DictionaryValue* sink_value = nullptr;
  EXPECT_TRUE(sinks_list_value->GetDictionary(0, &sink_value));

  std::string value;
  EXPECT_TRUE(sink_value->GetString("id", &value));
  EXPECT_EQ(sink_id, value);

  EXPECT_TRUE(sink_value->GetString("name", &value));
  EXPECT_EQ(sink_name, value);

  const base::ListValue* cast_modes_value = nullptr;
  EXPECT_TRUE(sink_value->GetList("castModes", &cast_modes_value));
  int cast_mode = -1;
  EXPECT_TRUE(cast_modes_value->GetInteger(0, &cast_mode));
  EXPECT_EQ(static_cast<int>(MediaCastMode::TAB_MIRROR), cast_mode);
}

TEST_F(MediaRouterWebUIMessageHandlerTest, UpdateRoutes) {
  MediaRoute::Id route_id("routeId123");
  MediaSink::Id sink_id("sinkId123");
  MediaSink sink(sink_id, "The sink");
  std::string description("This is a route");
  bool is_local = true;
  std::vector<MediaRoute> routes;
  routes.push_back(MediaRoute(route_id, MediaSource("mediaSource"),
                              MediaSink(sink_id, "The sink"), description,
                              is_local, ""));

  handler_.UpdateRoutes(routes);
  EXPECT_EQ(1u, web_ui_.call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_.call_data()[0];
  EXPECT_EQ("media_router.ui.setRouteList", call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::ListValue* routes_list_value = nullptr;
  EXPECT_TRUE(arg1->GetAsList(&routes_list_value));
  const base::DictionaryValue* route_value = nullptr;
  EXPECT_TRUE(routes_list_value->GetDictionary(0, &route_value));

  std::string value;
  EXPECT_TRUE(route_value->GetString("id", &value));
  EXPECT_EQ(route_id, value);
  EXPECT_TRUE(route_value->GetString("sinkId", &value));
  EXPECT_EQ(sink_id, value);
  EXPECT_TRUE(route_value->GetString("title", &value));
  EXPECT_EQ(description, value);

  bool actual_is_local = false;
  EXPECT_TRUE(route_value->GetBoolean("isLocal", &actual_is_local));
  EXPECT_EQ(is_local, actual_is_local);
}

TEST_F(MediaRouterWebUIMessageHandlerTest, OnCreateRouteResponseReceived) {
  MediaRoute::Id route_id("routeId123");
  MediaSink::Id sink_id("sinkId123");
  MediaSink sink(sink_id, "The sink");
  std::string description("This is a route");
  bool is_local = true;
  MediaRoute route(route_id, MediaSource("mediaSource"),
                   MediaSink(sink_id, "The sink"), description, is_local, "");

  handler_.OnCreateRouteResponseReceived(sink_id, &route);
  EXPECT_EQ(1u, web_ui_.call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_.call_data()[0];
  EXPECT_EQ("media_router.ui.onCreateRouteResponseReceived",
            call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::StringValue* sink_id_value = nullptr;
  EXPECT_TRUE(arg1->GetAsString(&sink_id_value));
  EXPECT_EQ(sink_id, sink_id_value->GetString());

  const base::Value* arg2 = call_data.arg2();
  const base::DictionaryValue* route_value = nullptr;
  EXPECT_TRUE(arg2->GetAsDictionary(&route_value));

  std::string value;
  EXPECT_TRUE(route_value->GetString("id", &value));
  EXPECT_EQ(route_id, value);
  EXPECT_TRUE(route_value->GetString("sinkId", &value));
  EXPECT_EQ(sink_id, value);
  EXPECT_TRUE(route_value->GetString("title", &value));
  EXPECT_EQ(description, value);

  bool actual_is_local = false;
  EXPECT_TRUE(route_value->GetBoolean("isLocal", &actual_is_local));
  EXPECT_EQ(is_local, actual_is_local);
}

TEST_F(MediaRouterWebUIMessageHandlerTest, UpdateIssue) {
  std::string issue_title("An issue");
  std::string issue_message("This is an issue");
  IssueAction default_action(IssueAction::TYPE_OK);
  std::vector<IssueAction> secondary_actions;
  secondary_actions.push_back(IssueAction(IssueAction::TYPE_DISMISS));
  MediaRoute::Id route_id("routeId123");
  bool is_blocking = true;
  Issue issue(issue_title, issue_message, default_action, secondary_actions,
              route_id, Issue::FATAL, is_blocking, "helpUrl");
  const Issue::Id& issue_id = issue.id();

  handler_.UpdateIssue(&issue);
  EXPECT_EQ(1u, web_ui_.call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_.call_data()[0];
  EXPECT_EQ("media_router.ui.setIssue", call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::DictionaryValue* issue_value = nullptr;
  EXPECT_TRUE(arg1->GetAsDictionary(&issue_value));

  std::string value;
  EXPECT_TRUE(issue_value->GetString("id", &value));
  EXPECT_EQ(issue_id, value);
  EXPECT_TRUE(issue_value->GetString("title", &value));
  EXPECT_EQ(issue_title, value);
  EXPECT_TRUE(issue_value->GetString("message", &value));
  EXPECT_EQ(issue_message, value);

  int action_type_num = -1;
  EXPECT_TRUE(issue_value->GetInteger("defaultActionType", &action_type_num));
  EXPECT_EQ(default_action.type(), action_type_num);
  EXPECT_TRUE(issue_value->GetInteger("secondaryActionType", &action_type_num));
  EXPECT_EQ(secondary_actions[0].type(), action_type_num);

  EXPECT_TRUE(issue_value->GetString("routeId", &value));
  EXPECT_EQ(route_id, value);

  bool actual_is_blocking = false;
  EXPECT_TRUE(issue_value->GetBoolean("isBlocking", &actual_is_blocking));
  EXPECT_EQ(is_blocking, actual_is_blocking);
}

}  // namespace media_router
