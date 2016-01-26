// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/media_router/media_router_test.h"
#include "chrome/browser/ui/webui/media_router/media_router_ui.h"
#include "chrome/browser/ui/webui/media_router/media_router_webui_message_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ReturnRef;

namespace media_router {

const char kProviderExtensionIdForTesting[] = "test_id";
const char kControllerPathForTesting[] = "test_path";

class MockMediaRouterUI : public MediaRouterUI {
 public:
  explicit MockMediaRouterUI(content::WebUI* web_ui)
      : MediaRouterUI(web_ui) {}
  ~MockMediaRouterUI() {}

  MOCK_CONST_METHOD0(GetRouteProviderExtensionId, const std::string&());
};

class TestingMediaRouterWebUIMessageHandler
    : public MediaRouterWebUIMessageHandler {
 public:
  TestingMediaRouterWebUIMessageHandler(content::WebUI* web_ui,
      MediaRouterUI* media_router_ui)
          : MediaRouterWebUIMessageHandler(media_router_ui) {
    set_web_ui(web_ui);
  }

  ~TestingMediaRouterWebUIMessageHandler() override { set_web_ui(nullptr); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingMediaRouterWebUIMessageHandler);
};

class MediaRouterWebUIMessageHandlerTest : public MediaRouterTest {
 public:
  MediaRouterWebUIMessageHandlerTest()
    : web_ui_(new content::TestWebUI()),
      provider_extension_id_(kProviderExtensionIdForTesting) {}
  ~MediaRouterWebUIMessageHandlerTest() override {}

  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    chrome::NewTab(browser());
    web_ui_->set_web_contents(
        browser()->tab_strip_model()->GetActiveWebContents());
    mock_media_router_ui_.reset(new MockMediaRouterUI(web_ui_.get()));
    handler_.reset(new TestingMediaRouterWebUIMessageHandler(
        web_ui_.get(), mock_media_router_ui_.get()));
  }

  void TearDown() override {
    handler_.reset();
    mock_media_router_ui_.reset();
    web_ui_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  const std::string& provider_extension_id() const {
    return provider_extension_id_;
  }

 protected:
  scoped_ptr<content::TestWebUI> web_ui_;
  scoped_ptr<MockMediaRouterUI> mock_media_router_ui_;
  scoped_ptr<MediaRouterWebUIMessageHandler> handler_;
  const std::string provider_extension_id_;
};

TEST_F(MediaRouterWebUIMessageHandlerTest, UpdateSinks) {
  MediaSink::Id sink_id("sinkId123");
  std::string sink_name("The sink");

  std::vector<MediaSinkWithCastModes> media_sink_with_cast_modes_list;
  MediaSinkWithCastModes media_sink_with_cast_modes(
      MediaSink(sink_id, sink_name, MediaSink::IconType::CAST));
  media_sink_with_cast_modes.cast_modes.insert(MediaCastMode::TAB_MIRROR);
  media_sink_with_cast_modes_list.push_back(media_sink_with_cast_modes);

  handler_->UpdateSinks(media_sink_with_cast_modes_list);
  EXPECT_EQ(1u, web_ui_->call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_->call_data()[0];
  EXPECT_EQ("media_router.ui.setSinkList", call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::ListValue* sinks_list_value = nullptr;
  ASSERT_TRUE(arg1->GetAsList(&sinks_list_value));
  const base::DictionaryValue* sink_value = nullptr;
  ASSERT_TRUE(sinks_list_value->GetDictionary(0, &sink_value));

  std::string value;
  EXPECT_TRUE(sink_value->GetString("id", &value));
  EXPECT_EQ(sink_id, value);

  EXPECT_TRUE(sink_value->GetString("name", &value));
  EXPECT_EQ(sink_name, value);

  int cast_mode_bits = -1;
  ASSERT_TRUE(sink_value->GetInteger("castModes", &cast_mode_bits));
  EXPECT_EQ(static_cast<int>(MediaCastMode::TAB_MIRROR), cast_mode_bits);
}

TEST_F(MediaRouterWebUIMessageHandlerTest, UpdateRoutes) {
  MediaRoute::Id route_id("routeId123");
  MediaSink::Id sink_id("sinkId123");
  MediaSink sink(sink_id, "The sink", MediaSink::IconType::CAST);
  MediaSource media_source("mediaSource");
  std::string description("This is a route");
  bool is_local = true;
  std::vector<MediaRoute> routes;
  routes.push_back(MediaRoute(route_id, media_source, sink_id,
                              description, is_local, kControllerPathForTesting,
                              true));
  std::vector<MediaRoute::Id> joinable_route_ids;
  joinable_route_ids.push_back(route_id);

  EXPECT_CALL(*mock_media_router_ui_, GetRouteProviderExtensionId()).WillOnce(
      ReturnRef(provider_extension_id()));
  handler_->UpdateRoutes(routes, joinable_route_ids);
  EXPECT_EQ(1u, web_ui_->call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_->call_data()[0];
  EXPECT_EQ("media_router.ui.setRouteList", call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::ListValue* routes_list_value = nullptr;
  ASSERT_TRUE(arg1->GetAsList(&routes_list_value));
  const base::DictionaryValue* route_value = nullptr;
  ASSERT_TRUE(routes_list_value->GetDictionary(0, &route_value));

  std::string value;
  EXPECT_TRUE(route_value->GetString("id", &value));
  EXPECT_EQ(route_id, value);
  EXPECT_TRUE(route_value->GetString("sinkId", &value));
  EXPECT_EQ(sink_id, value);
  EXPECT_TRUE(route_value->GetString("description", &value));
  EXPECT_EQ(description, value);

  bool actual_is_local = false;
  EXPECT_TRUE(route_value->GetBoolean("isLocal", &actual_is_local));
  EXPECT_EQ(is_local, actual_is_local);
  bool actual_can_join = false;
  EXPECT_TRUE(route_value->GetBoolean("canJoin", &actual_can_join));
  EXPECT_TRUE(actual_can_join);

  std::string custom_controller_path;
  EXPECT_TRUE(route_value->GetString("customControllerPath",
                                     &custom_controller_path));
  std::string expected_path = base::StringPrintf("%s://%s/%s",
                                  extensions::kExtensionScheme,
                                  kProviderExtensionIdForTesting,
                                  kControllerPathForTesting);
  EXPECT_EQ(expected_path, custom_controller_path);
}

TEST_F(MediaRouterWebUIMessageHandlerTest, OnCreateRouteResponseReceived) {
  MediaRoute::Id route_id("routeId123");
  MediaSink::Id sink_id("sinkId123");
  MediaSink sink(sink_id, "The sink", MediaSink::IconType::CAST);
  std::string description("This is a route");
  bool is_local = true;
  MediaRoute route(route_id, MediaSource("mediaSource"), sink_id, description,
                   is_local, "", true);

  handler_->OnCreateRouteResponseReceived(sink_id, route.media_route_id());
  EXPECT_EQ(1u, web_ui_->call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_->call_data()[0];
  EXPECT_EQ("media_router.ui.onCreateRouteResponseReceived",
            call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::StringValue* sink_id_value = nullptr;
  ASSERT_TRUE(arg1->GetAsString(&sink_id_value));
  EXPECT_EQ(sink_id, sink_id_value->GetString());

  const base::Value* arg2 = call_data.arg2();
  const base::StringValue* route_id_value = nullptr;
  ASSERT_TRUE(arg2->GetAsString(&route_id_value));
  EXPECT_EQ(route_id, route_id_value->GetString());
}

TEST_F(MediaRouterWebUIMessageHandlerTest, UpdateIssue) {
  std::string issue_title("An issue");
  std::string issue_message("This is an issue");
  IssueAction default_action(IssueAction::TYPE_LEARN_MORE);
  std::vector<IssueAction> secondary_actions;
  secondary_actions.push_back(IssueAction(IssueAction::TYPE_DISMISS));
  MediaRoute::Id route_id("routeId123");
  bool is_blocking = true;
  Issue issue(issue_title, issue_message, default_action, secondary_actions,
              route_id, Issue::FATAL, is_blocking, "helpUrl");
  const Issue::Id& issue_id = issue.id();

  handler_->UpdateIssue(&issue);
  EXPECT_EQ(1u, web_ui_->call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_->call_data()[0];
  EXPECT_EQ("media_router.ui.setIssue", call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::DictionaryValue* issue_value = nullptr;
  ASSERT_TRUE(arg1->GetAsDictionary(&issue_value));

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
