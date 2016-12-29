// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/media_router/media_router_ui.h"
#include "chrome/browser/ui/webui/media_router/media_router_web_ui_test.h"
#include "chrome/browser/ui/webui/media_router/media_router_webui_message_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::ReturnRef;

namespace media_router {

const char kProviderExtensionIdForTesting[] = "test_id";
const char kControllerPathForTesting[] = "test_path";
const char kUserEmailForTesting[] = "nobody@example.com";
const char kUserDomainForTesting[] = "example.com";

class MockMediaRouterUI : public MediaRouterUI {
 public:
  explicit MockMediaRouterUI(content::WebUI* web_ui)
      : MediaRouterUI(web_ui) {}
  ~MockMediaRouterUI() {}

  MOCK_METHOD0(UIInitialized, void());
  MOCK_CONST_METHOD0(UserSelectedTabMirroringForCurrentOrigin, bool());
  MOCK_METHOD1(RecordCastModeSelection, void(MediaCastMode cast_mode));
  MOCK_CONST_METHOD0(cast_modes, const std::set<MediaCastMode>&());
  MOCK_CONST_METHOD0(GetRouteProviderExtensionId, const std::string&());
};

class TestMediaRouterWebUIMessageHandler
    : public MediaRouterWebUIMessageHandler {
 public:
  explicit TestMediaRouterWebUIMessageHandler(MediaRouterUI* media_router_ui)
      : MediaRouterWebUIMessageHandler(media_router_ui),
        email_(kUserEmailForTesting),
        domain_(kUserDomainForTesting) {}
  ~TestMediaRouterWebUIMessageHandler() override = default;

  AccountInfo GetAccountInfo() override {
    AccountInfo info = AccountInfo();
    info.account_id = info.gaia = info.email = email_;
    info.hosted_domain = domain_;
    info.full_name = info.given_name = "name";
    info.locale = "locale";
    info.picture_url = "picture";

    return info;
  }

  void SetEmailAndDomain(const std::string& email, const std::string& domain) {
    email_ = email;
    domain_ = domain;
  }

 private:
  std::string email_;
  std::string domain_;
};

class MediaRouterWebUIMessageHandlerTest : public MediaRouterWebUITest {
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
    handler_.reset(
        new TestMediaRouterWebUIMessageHandler(mock_media_router_ui_.get()));
    handler_->SetWebUIForTest(web_ui_.get());
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
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<MockMediaRouterUI> mock_media_router_ui_;
  std::unique_ptr<TestMediaRouterWebUIMessageHandler> handler_;
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
  EXPECT_EQ("media_router.ui.setSinkListAndIdentity",
            call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::DictionaryValue* sinks_with_identity_value = nullptr;
  ASSERT_TRUE(arg1->GetAsDictionary(&sinks_with_identity_value));

  // Email is not displayed if there is no sinks with domain.
  bool show_email = false;
  bool actual_show_email = false;
  EXPECT_TRUE(
      sinks_with_identity_value->GetBoolean("showEmail", &actual_show_email));
  EXPECT_EQ(show_email, actual_show_email);

  // Domain is not displayed if there is no sinks with domain.
  bool show_domain = false;
  bool actual_show_domain = false;
  EXPECT_TRUE(
      sinks_with_identity_value->GetBoolean("showDomain", &actual_show_domain));
  EXPECT_EQ(show_domain, actual_show_domain);

  const base::ListValue* sinks_list_value = nullptr;
  ASSERT_TRUE(sinks_with_identity_value->GetList("sinks", &sinks_list_value));
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

TEST_F(MediaRouterWebUIMessageHandlerTest, UpdateSinksWithIdentity) {
  MediaSink::Id sink_id("sinkId123");
  std::string sink_name("The sink");

  std::vector<MediaSinkWithCastModes> media_sink_with_cast_modes_list;
  MediaSinkWithCastModes media_sink_with_cast_modes(
      MediaSink(sink_id, sink_name, MediaSink::IconType::CAST));
  media_sink_with_cast_modes.sink.set_domain(kUserDomainForTesting);
  media_sink_with_cast_modes.cast_modes.insert(MediaCastMode::TAB_MIRROR);
  media_sink_with_cast_modes_list.push_back(media_sink_with_cast_modes);

  handler_->UpdateSinks(media_sink_with_cast_modes_list);
  EXPECT_EQ(1u, web_ui_->call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_->call_data()[0];
  EXPECT_EQ("media_router.ui.setSinkListAndIdentity",
            call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::DictionaryValue* sinks_with_identity_value = nullptr;
  ASSERT_TRUE(arg1->GetAsDictionary(&sinks_with_identity_value));

  bool show_email = true;
  bool actual_show_email = false;
  EXPECT_TRUE(
      sinks_with_identity_value->GetBoolean("showEmail", &actual_show_email));
  EXPECT_EQ(show_email, actual_show_email);

  // Sink domain is not displayed if it matches user domain.
  bool show_domain = false;
  bool actual_show_domain = false;
  EXPECT_TRUE(
      sinks_with_identity_value->GetBoolean("showDomain", &actual_show_domain));
  EXPECT_EQ(show_domain, actual_show_domain);

  std::string value;
  EXPECT_TRUE(sinks_with_identity_value->GetString("userEmail", &value));
  EXPECT_EQ(kUserEmailForTesting, value);
}

TEST_F(MediaRouterWebUIMessageHandlerTest,
       UpdateSinksWithIdentityAndPseudoSink) {
  MediaSink::Id sink_id("pseudo:sinkId123");
  std::string sink_name("The sink");

  std::vector<MediaSinkWithCastModes> media_sink_with_cast_modes_list;
  MediaSinkWithCastModes media_sink_with_cast_modes(
      MediaSink(sink_id, sink_name, MediaSink::IconType::CAST));
  media_sink_with_cast_modes.sink.set_domain(kUserDomainForTesting);
  media_sink_with_cast_modes.cast_modes.insert(MediaCastMode::TAB_MIRROR);
  media_sink_with_cast_modes_list.push_back(media_sink_with_cast_modes);

  handler_->UpdateSinks(media_sink_with_cast_modes_list);
  EXPECT_EQ(1u, web_ui_->call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_->call_data()[0];
  EXPECT_EQ("media_router.ui.setSinkListAndIdentity",
            call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::DictionaryValue* sinks_with_identity_value = nullptr;
  ASSERT_TRUE(arg1->GetAsDictionary(&sinks_with_identity_value));

  bool show_email = false;
  bool actual_show_email = true;
  EXPECT_TRUE(
      sinks_with_identity_value->GetBoolean("showEmail", &actual_show_email));
  EXPECT_EQ(show_email, actual_show_email);
}

TEST_F(MediaRouterWebUIMessageHandlerTest, UpdateSinksWithIdentityAndDomain) {
  MediaSink::Id sink_id("sinkId123");
  std::string sink_name("The sink");
  std::string domain_name("google.com");

  std::vector<MediaSinkWithCastModes> media_sink_with_cast_modes_list;
  MediaSinkWithCastModes media_sink_with_cast_modes(
      MediaSink(sink_id, sink_name, MediaSink::IconType::CAST));
  media_sink_with_cast_modes.sink.set_domain(domain_name);
  media_sink_with_cast_modes.cast_modes.insert(MediaCastMode::TAB_MIRROR);
  media_sink_with_cast_modes_list.push_back(media_sink_with_cast_modes);

  handler_->UpdateSinks(media_sink_with_cast_modes_list);
  EXPECT_EQ(1u, web_ui_->call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_->call_data()[0];
  EXPECT_EQ("media_router.ui.setSinkListAndIdentity",
            call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::DictionaryValue* sinks_with_identity_value = nullptr;
  ASSERT_TRUE(arg1->GetAsDictionary(&sinks_with_identity_value));

  // Domain is displayed for sinks with domains that are not the user domain.
  bool show_domain = true;
  bool actual_show_domain = false;
  EXPECT_TRUE(
      sinks_with_identity_value->GetBoolean("showDomain", &actual_show_domain));
  EXPECT_EQ(show_domain, actual_show_domain);
}

TEST_F(MediaRouterWebUIMessageHandlerTest, UpdateSinksWithNoDomain) {
  MediaSink::Id sink_id("sinkId123");
  std::string sink_name("The sink");
  std::string user_email("nobody@gmail.com");
  std::string user_domain("NO_HOSTED_DOMAIN");
  std::string domain_name("default");

  handler_->SetEmailAndDomain(user_email, user_domain);

  std::vector<MediaSinkWithCastModes> media_sink_with_cast_modes_list;
  MediaSinkWithCastModes media_sink_with_cast_modes(
      MediaSink(sink_id, sink_name, MediaSink::IconType::CAST));
  media_sink_with_cast_modes.sink.set_domain(domain_name);
  media_sink_with_cast_modes.cast_modes.insert(MediaCastMode::TAB_MIRROR);
  media_sink_with_cast_modes_list.push_back(media_sink_with_cast_modes);

  handler_->UpdateSinks(media_sink_with_cast_modes_list);
  EXPECT_EQ(1u, web_ui_->call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_->call_data()[0];
  EXPECT_EQ("media_router.ui.setSinkListAndIdentity",
            call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::DictionaryValue* sinks_with_identity_value = nullptr;
  ASSERT_TRUE(arg1->GetAsDictionary(&sinks_with_identity_value));

  const base::ListValue* sinks_list_value = nullptr;
  ASSERT_TRUE(sinks_with_identity_value->GetList("sinks", &sinks_list_value));
  const base::DictionaryValue* sink_value = nullptr;
  ASSERT_TRUE(sinks_list_value->GetDictionary(0, &sink_value));

  // Domain should not be shown if there were only default sink domains.
  bool show_domain = false;
  bool actual_show_domain = false;
  EXPECT_TRUE(
      sinks_with_identity_value->GetBoolean("showDomain", &actual_show_domain));
  EXPECT_EQ(show_domain, actual_show_domain);

  // Sink domain should be empty if user has no hosted domain.
  std::string value;
  EXPECT_TRUE(sink_value->GetString("domain", &value));
  EXPECT_EQ(std::string(), value);
}

TEST_F(MediaRouterWebUIMessageHandlerTest, UpdateSinksWithDefaultDomain) {
  MediaSink::Id sink_id("sinkId123");
  std::string sink_name("The sink");
  std::string domain_name("default");

  std::vector<MediaSinkWithCastModes> media_sink_with_cast_modes_list;
  MediaSinkWithCastModes media_sink_with_cast_modes(
      MediaSink(sink_id, sink_name, MediaSink::IconType::CAST));
  media_sink_with_cast_modes.sink.set_domain(domain_name);
  media_sink_with_cast_modes.cast_modes.insert(MediaCastMode::TAB_MIRROR);
  media_sink_with_cast_modes_list.push_back(media_sink_with_cast_modes);

  handler_->UpdateSinks(media_sink_with_cast_modes_list);
  EXPECT_EQ(1u, web_ui_->call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_->call_data()[0];
  EXPECT_EQ("media_router.ui.setSinkListAndIdentity",
            call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::DictionaryValue* sinks_with_identity_value = nullptr;
  ASSERT_TRUE(arg1->GetAsDictionary(&sinks_with_identity_value));

  const base::ListValue* sinks_list_value = nullptr;
  ASSERT_TRUE(sinks_with_identity_value->GetList("sinks", &sinks_list_value));
  const base::DictionaryValue* sink_value = nullptr;
  ASSERT_TRUE(sinks_list_value->GetDictionary(0, &sink_value));

  // Domain should not be shown if there were only default sink domains.
  bool show_domain = false;
  bool actual_show_domain = false;
  EXPECT_TRUE(
      sinks_with_identity_value->GetBoolean("showDomain", &actual_show_domain));
  EXPECT_EQ(show_domain, actual_show_domain);

  // Sink domain should be updated from 'default' to user domain.
  std::string value;
  EXPECT_TRUE(sink_value->GetString("domain", &value));
  EXPECT_EQ(kUserDomainForTesting, value);
}

TEST_F(MediaRouterWebUIMessageHandlerTest, UpdateRoutes) {
  MediaRoute::Id route_id("routeId123");
  MediaSink::Id sink_id("sinkId123");
  MediaSink sink(sink_id, "The sink", MediaSink::IconType::CAST);
  MediaSource media_source("mediaSource");
  std::string description("This is a route");
  bool is_local = true;
  std::vector<MediaRoute> routes;
  routes.push_back(MediaRoute(route_id, media_source, sink_id, description,
                              is_local, kControllerPathForTesting, true));
  std::vector<MediaRoute::Id> joinable_route_ids;
  joinable_route_ids.push_back(route_id);
  std::unordered_map<MediaRoute::Id, MediaCastMode> current_cast_modes;
  current_cast_modes.insert(std::make_pair(route_id, MediaCastMode::DEFAULT));

  EXPECT_CALL(*mock_media_router_ui_, GetRouteProviderExtensionId()).WillOnce(
      ReturnRef(provider_extension_id()));
  handler_->UpdateRoutes(routes, joinable_route_ids, current_cast_modes);
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
  int actual_current_cast_mode = -1;
  EXPECT_TRUE(
      route_value->GetInteger("currentCastMode", &actual_current_cast_mode));
  EXPECT_EQ(MediaCastMode::DEFAULT, actual_current_cast_mode);

  std::string custom_controller_path;
  EXPECT_TRUE(route_value->GetString("customControllerPath",
                                     &custom_controller_path));
  std::string expected_path = base::StringPrintf("%s://%s/%s",
                                  extensions::kExtensionScheme,
                                  kProviderExtensionIdForTesting,
                                  kControllerPathForTesting);
  EXPECT_EQ(expected_path, custom_controller_path);
}

TEST_F(MediaRouterWebUIMessageHandlerTest, UpdateRoutesIncognito) {
  handler_->set_incognito_for_test(true);

  MediaRoute::Id route_id("routeId123");
  MediaSink::Id sink_id("sinkId123");
  MediaSink sink(sink_id, "The sink", MediaSink::IconType::CAST);
  MediaSource media_source("mediaSource");
  std::string description("This is a route");
  bool is_local = true;
  std::vector<MediaRoute> routes;
  routes.push_back(MediaRoute(route_id, media_source, sink_id, description,
                              is_local, kControllerPathForTesting, true));

  EXPECT_CALL(*mock_media_router_ui_, GetRouteProviderExtensionId())
      .WillOnce(ReturnRef(provider_extension_id()));
  handler_->UpdateRoutes(routes, std::vector<MediaRoute::Id>(),
                         std::unordered_map<MediaRoute::Id, MediaCastMode>());
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
  EXPECT_FALSE(actual_can_join);
  int actual_current_cast_mode = -1;
  EXPECT_FALSE(
      route_value->GetInteger("currentCastMode", &actual_current_cast_mode));

  std::string custom_controller_path;
  EXPECT_FALSE(
      route_value->GetString("customControllerPath", &custom_controller_path));
}

TEST_F(MediaRouterWebUIMessageHandlerTest, OnCreateRouteResponseReceived) {
  MediaRoute::Id route_id("routeId123");
  MediaSink::Id sink_id("sinkId123");
  MediaSink sink(sink_id, "The sink", MediaSink::IconType::CAST);
  std::string description("This is a route");
  bool is_local = true;
  bool is_for_display = true;
  bool incognito = false;
  MediaRoute route(route_id, MediaSource("mediaSource"), sink_id, description,
                   is_local, kControllerPathForTesting, is_for_display);
  route.set_incognito(incognito);

  EXPECT_CALL(*mock_media_router_ui_, GetRouteProviderExtensionId())
      .WillOnce(ReturnRef(provider_extension_id()));
  handler_->OnCreateRouteResponseReceived(sink_id, &route);
  EXPECT_EQ(1u, web_ui_->call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_->call_data()[0];
  EXPECT_EQ("media_router.ui.onCreateRouteResponseReceived",
            call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::StringValue* sink_id_value = nullptr;
  ASSERT_TRUE(arg1->GetAsString(&sink_id_value));
  EXPECT_EQ(sink_id, sink_id_value->GetString());

  const base::Value* arg2 = call_data.arg2();
  const base::DictionaryValue* route_value = nullptr;
  ASSERT_TRUE(arg2->GetAsDictionary(&route_value));
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

  std::string actual_path;
  EXPECT_TRUE(route_value->GetString("customControllerPath", &actual_path));
  std::string expected_path = base::StringPrintf(
      "%s://%s/%s", extensions::kExtensionScheme,
      kProviderExtensionIdForTesting, kControllerPathForTesting);
  EXPECT_EQ(expected_path, actual_path);

  const base::Value* arg3 = call_data.arg3();
  bool route_for_display = false;
  ASSERT_TRUE(arg3->GetAsBoolean(&route_for_display));
  EXPECT_EQ(is_for_display, route_for_display);
}

TEST_F(MediaRouterWebUIMessageHandlerTest,
       OnCreateRouteResponseReceivedIncognito) {
  handler_->set_incognito_for_test(true);

  MediaRoute::Id route_id("routeId123");
  MediaSink::Id sink_id("sinkId123");
  MediaSink sink(sink_id, "The sink", MediaSink::IconType::CAST);
  std::string description("This is a route");
  bool is_local = true;
  bool is_for_display = true;
  bool incognito = true;
  MediaRoute route(route_id, MediaSource("mediaSource"), sink_id, description,
                   is_local, kControllerPathForTesting, is_for_display);
  route.set_incognito(incognito);

  EXPECT_CALL(*mock_media_router_ui_, GetRouteProviderExtensionId()).WillOnce(
      ReturnRef(provider_extension_id()));
  handler_->OnCreateRouteResponseReceived(sink_id, &route);
  EXPECT_EQ(1u, web_ui_->call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_->call_data()[0];
  EXPECT_EQ("media_router.ui.onCreateRouteResponseReceived",
            call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::StringValue* sink_id_value = nullptr;
  ASSERT_TRUE(arg1->GetAsString(&sink_id_value));
  EXPECT_EQ(sink_id, sink_id_value->GetString());

  const base::Value* arg2 = call_data.arg2();
  const base::DictionaryValue* route_value = nullptr;
  ASSERT_TRUE(arg2->GetAsDictionary(&route_value));
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

  std::string actual_path;
  EXPECT_FALSE(route_value->GetString("customControllerPath", &actual_path));

  const base::Value* arg3 = call_data.arg3();
  bool route_for_display = false;
  ASSERT_TRUE(arg3->GetAsBoolean(&route_for_display));
  EXPECT_EQ(is_for_display, route_for_display);
}

TEST_F(MediaRouterWebUIMessageHandlerTest, UpdateIssue) {
  std::string issue_title("An issue");
  std::string issue_message("This is an issue");
  IssueInfo::Action default_action = IssueInfo::Action::LEARN_MORE;
  std::vector<IssueInfo::Action> secondary_actions;
  secondary_actions.push_back(IssueInfo::Action::DISMISS);
  MediaRoute::Id route_id("routeId123");
  IssueInfo info(issue_title, default_action, IssueInfo::Severity::FATAL);
  info.message = issue_message;
  info.secondary_actions = secondary_actions;
  info.route_id = route_id;
  Issue issue(info);
  const Issue::Id& issue_id = issue.id();

  handler_->UpdateIssue(issue);
  EXPECT_EQ(1u, web_ui_->call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui_->call_data()[0];
  EXPECT_EQ("media_router.ui.setIssue", call_data.function_name());
  const base::Value* arg1 = call_data.arg1();
  const base::DictionaryValue* issue_value = nullptr;
  ASSERT_TRUE(arg1->GetAsDictionary(&issue_value));

  // Initialized to invalid issue id.
  int actual_issue_id = -1;
  std::string value;
  EXPECT_TRUE(issue_value->GetInteger("id", &actual_issue_id));
  EXPECT_EQ(issue_id, actual_issue_id);
  EXPECT_TRUE(issue_value->GetString("title", &value));
  EXPECT_EQ(issue_title, value);
  EXPECT_TRUE(issue_value->GetString("message", &value));
  EXPECT_EQ(issue_message, value);

  // Initialized to invalid action type.
  int action_type_num = -1;
  EXPECT_TRUE(issue_value->GetInteger("defaultActionType", &action_type_num));
  EXPECT_EQ(static_cast<int>(default_action), action_type_num);
  EXPECT_TRUE(issue_value->GetInteger("secondaryActionType", &action_type_num));
  EXPECT_EQ(static_cast<int>(secondary_actions[0]), action_type_num);

  EXPECT_TRUE(issue_value->GetString("routeId", &value));
  EXPECT_EQ(route_id, value);

  // The issue is blocking since it is FATAL.
  bool actual_is_blocking = false;
  EXPECT_TRUE(issue_value->GetBoolean("isBlocking", &actual_is_blocking));
  EXPECT_TRUE(actual_is_blocking);
}

TEST_F(MediaRouterWebUIMessageHandlerTest, RecordCastModeSelection) {
  base::ListValue args;
  args.AppendInteger(MediaCastMode::DEFAULT);
  EXPECT_CALL(*mock_media_router_ui_.get(),
              RecordCastModeSelection(MediaCastMode::DEFAULT))
      .Times(1);
  handler_->OnReportSelectedCastMode(&args);

  args.Clear();
  args.AppendInteger(MediaCastMode::TAB_MIRROR);
  EXPECT_CALL(*mock_media_router_ui_.get(),
              RecordCastModeSelection(MediaCastMode::TAB_MIRROR))
      .Times(1);
  handler_->OnReportSelectedCastMode(&args);
}

TEST_F(MediaRouterWebUIMessageHandlerTest, RetrieveCastModeSelection) {
  base::ListValue args;
  std::set<MediaCastMode> cast_modes = {MediaCastMode::TAB_MIRROR};
  EXPECT_CALL(*mock_media_router_ui_, GetRouteProviderExtensionId())
      .WillRepeatedly(ReturnRef(provider_extension_id()));
  EXPECT_CALL(*mock_media_router_ui_, cast_modes())
      .WillRepeatedly(ReturnRef(cast_modes));

  EXPECT_CALL(*mock_media_router_ui_,
              UserSelectedTabMirroringForCurrentOrigin())
      .WillOnce(Return(true));
  handler_->OnRequestInitialData(&args);
  const content::TestWebUI::CallData& call_data1 = *web_ui_->call_data()[0];
  ASSERT_EQ("media_router.ui.setInitialData", call_data1.function_name());
  const base::DictionaryValue* initial_data = nullptr;
  ASSERT_TRUE(call_data1.arg1()->GetAsDictionary(&initial_data));
  bool use_tab_mirroring = false;
  EXPECT_TRUE(initial_data->GetBoolean("useTabMirroring", &use_tab_mirroring));
  EXPECT_TRUE(use_tab_mirroring);

  EXPECT_CALL(*mock_media_router_ui_,
              UserSelectedTabMirroringForCurrentOrigin())
      .WillOnce(Return(false));
  handler_->OnRequestInitialData(&args);
  const content::TestWebUI::CallData& call_data2 = *web_ui_->call_data()[1];
  ASSERT_EQ("media_router.ui.setInitialData", call_data2.function_name());
  ASSERT_TRUE(call_data2.arg1()->GetAsDictionary(&initial_data));
  EXPECT_TRUE(initial_data->GetBoolean("useTabMirroring", &use_tab_mirroring));
  EXPECT_FALSE(use_tab_mirroring);
}

}  // namespace media_router
