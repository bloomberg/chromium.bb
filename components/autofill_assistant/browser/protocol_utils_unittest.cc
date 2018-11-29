// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/protocol_utils.h"

#include "base/macros.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

ClientContextProto CreateClientContextProto() {
  ClientContextProto context;
  context.mutable_chrome()->set_chrome_version("v");
  return context;
}

void AssertClientContext(const ClientContextProto& context) {
  EXPECT_EQ("v", context.chrome().chrome_version());
}

TEST(ProtocolUtilsTest, NoScripts) {
  std::vector<std::unique_ptr<Script>> scripts;
  EXPECT_TRUE(ProtocolUtils::ParseScripts("", &scripts));
  EXPECT_THAT(scripts, IsEmpty());
}

TEST(ProtocolUtilsTest, SomeInvalidScripts) {
  SupportsScriptResponseProto proto;

  // 2 Invalid scripts, 1 valid one, with no preconditions.
  proto.add_scripts()->mutable_presentation()->set_name("missing path");
  proto.add_scripts()->set_path("missing name");
  SupportedScriptProto* script = proto.add_scripts();
  script->set_path("ok");
  script->mutable_presentation()->set_name("ok name");

  // Only the valid script is returned.
  std::vector<std::unique_ptr<Script>> scripts;
  std::string proto_str;
  proto.SerializeToString(&proto_str);
  EXPECT_TRUE(ProtocolUtils::ParseScripts(proto_str, &scripts));
  ASSERT_THAT(scripts, SizeIs(1));
  EXPECT_EQ("ok", scripts[0]->handle.path);
  EXPECT_EQ("ok name", scripts[0]->handle.name);
  EXPECT_NE(nullptr, scripts[0]->precondition);
}

TEST(ProtocolUtilsTest, OneFullyFeaturedScript) {
  SupportsScriptResponseProto proto;

  SupportedScriptProto* script = proto.add_scripts();
  script->set_path("path");
  auto* presentation = script->mutable_presentation();
  presentation->set_name("name");
  presentation->set_autostart(true);
  presentation->set_initial_prompt("prompt");
  presentation->mutable_precondition()->add_domain("www.example.com");

  std::vector<std::unique_ptr<Script>> scripts;
  std::string proto_str;
  proto.SerializeToString(&proto_str);
  EXPECT_TRUE(ProtocolUtils::ParseScripts(proto_str, &scripts));
  ASSERT_THAT(scripts, SizeIs(1));
  EXPECT_EQ("path", scripts[0]->handle.path);
  EXPECT_EQ("name", scripts[0]->handle.name);
  EXPECT_EQ("prompt", scripts[0]->handle.initial_prompt);
  EXPECT_TRUE(scripts[0]->handle.autostart);
  EXPECT_NE(nullptr, scripts[0]->precondition);
}

TEST(ProtocolUtilsTest, AllowInterruptsWithNoName) {
  SupportsScriptResponseProto proto;

  SupportedScriptProto* script = proto.add_scripts();
  script->set_path("path");
  auto* presentation = script->mutable_presentation();
  presentation->set_autostart(true);
  presentation->set_initial_prompt("prompt");
  presentation->set_interrupt(true);
  presentation->mutable_precondition()->add_domain("www.example.com");

  std::vector<std::unique_ptr<Script>> scripts;
  std::string proto_str;
  proto.SerializeToString(&proto_str);
  EXPECT_TRUE(ProtocolUtils::ParseScripts(proto_str, &scripts));
  ASSERT_THAT(scripts, SizeIs(1));
  EXPECT_EQ("path", scripts[0]->handle.path);
  EXPECT_EQ("", scripts[0]->handle.name);
  EXPECT_TRUE(scripts[0]->handle.interrupt);
}

TEST(ProtocolUtilsTest, CreateInitialScriptActionsRequest) {
  std::map<std::string, std::string> parameters;
  parameters["a"] = "b";
  parameters["c"] = "d";

  ScriptActionRequestProto request;
  EXPECT_TRUE(
      request.ParseFromString(ProtocolUtils::CreateInitialScriptActionsRequest(
          "script_path", GURL("http://example.com/"), parameters,
          "global_payload", "script_payload", CreateClientContextProto())));

  AssertClientContext(request.client_context());

  const InitialScriptActionsRequestProto& initial = request.initial_request();
  EXPECT_THAT(initial.query().script_path(), ElementsAre("script_path"));
  EXPECT_EQ(initial.query().url(), "http://example.com/");
  ASSERT_EQ(2, initial.script_parameters_size());
  EXPECT_EQ("a", initial.script_parameters(0).name());
  EXPECT_EQ("b", initial.script_parameters(0).value());
  EXPECT_EQ("c", initial.script_parameters(1).name());
  EXPECT_EQ("d", initial.script_parameters(1).value());
  EXPECT_EQ("global_payload", request.global_payload());
  EXPECT_EQ("script_payload", request.script_payload());
}

TEST(ProtocolUtilsTest, CreateGetScriptsRequest) {
  std::map<std::string, std::string> parameters;
  parameters["a"] = "b";
  parameters["c"] = "d";

  SupportsScriptRequestProto request;
  EXPECT_TRUE(request.ParseFromString(ProtocolUtils::CreateGetScriptsRequest(
      GURL("http://example.com/"), parameters, CreateClientContextProto())));

  AssertClientContext(request.client_context());

  EXPECT_EQ("http://example.com/", request.url());
  ASSERT_EQ(2, request.script_parameters_size());
  EXPECT_EQ("a", request.script_parameters(0).name());
  EXPECT_EQ("b", request.script_parameters(0).value());
  EXPECT_EQ("c", request.script_parameters(1).name());
  EXPECT_EQ("d", request.script_parameters(1).value());
}

TEST(ProtocolUtilsTest, AddScriptIgnoreInvalid) {
  SupportedScriptProto script_proto;
  std::vector<std::unique_ptr<Script>> scripts;
  ProtocolUtils::AddScript(script_proto, &scripts);
  EXPECT_TRUE(scripts.empty());
}

TEST(ProtocolUtilsTest, AddScriptValid) {
  SupportedScriptProto script_proto;
  script_proto.set_path("path");
  auto* presentation = script_proto.mutable_presentation();
  presentation->set_name("name");
  presentation->set_autostart(true);
  presentation->set_initial_prompt("prompt");
  presentation->mutable_precondition()->add_domain("www.example.com");

  std::vector<std::unique_ptr<Script>> scripts;
  ProtocolUtils::AddScript(script_proto, &scripts);
  std::unique_ptr<Script> script = std::move(scripts[0]);

  EXPECT_NE(nullptr, script);
  EXPECT_EQ("path", script->handle.path);
  EXPECT_EQ("name", script->handle.name);
  EXPECT_EQ("prompt", script->handle.initial_prompt);
  EXPECT_TRUE(script->handle.autostart);
  EXPECT_NE(nullptr, script->precondition);
}

TEST(ProtocolUtilsTest, ParseActionsParseError) {
  bool unused;
  std::vector<std::unique_ptr<Action>> unused_actions;
  std::vector<std::unique_ptr<Script>> unused_scripts;
  EXPECT_FALSE(ProtocolUtils::ParseActions(
      "invalid", nullptr, nullptr, &unused_actions, &unused_scripts, &unused));
}

TEST(ProtocolUtilsTest, ParseActionsValid) {
  ActionsResponseProto proto;
  proto.set_global_payload("global_payload");
  proto.set_script_payload("script_payload");
  proto.add_actions()->mutable_tell();
  proto.add_actions()->mutable_click();

  std::string proto_str;
  proto.SerializeToString(&proto_str);

  std::string global_payload;
  std::string script_payload;
  bool should_update_scripts = true;
  std::vector<std::unique_ptr<Action>> actions;
  std::vector<std::unique_ptr<Script>> scripts;

  EXPECT_TRUE(ProtocolUtils::ParseActions(proto_str, &global_payload,
                                          &script_payload, &actions, &scripts,
                                          &should_update_scripts));
  EXPECT_EQ("global_payload", global_payload);
  EXPECT_EQ("script_payload", script_payload);
  EXPECT_THAT(actions, SizeIs(2));
  EXPECT_FALSE(should_update_scripts);
  EXPECT_TRUE(scripts.empty());
}

TEST(ProtocolUtilsTest, ParseActionsEmptyUpdateScriptList) {
  ActionsResponseProto proto;
  proto.mutable_update_script_list();

  std::string proto_str;
  proto.SerializeToString(&proto_str);

  bool should_update_scripts = false;
  std::vector<std::unique_ptr<Script>> scripts;
  std::vector<std::unique_ptr<Action>> unused_actions;

  EXPECT_TRUE(ProtocolUtils::ParseActions(
      proto_str, /* global_payload= */ nullptr, /* script_payload */ nullptr,
      &unused_actions, &scripts, &should_update_scripts));
  EXPECT_TRUE(should_update_scripts);
  EXPECT_TRUE(scripts.empty());
}

TEST(ProtocolUtilsTest, ParseActionsUpdateScriptListFullFeatured) {
  ActionsResponseProto proto;
  auto* script_list = proto.mutable_update_script_list();
  auto* script_a = script_list->add_scripts();
  script_a->set_path("a");
  auto* presentation = script_a->mutable_presentation();
  presentation->set_name("name");
  presentation->mutable_precondition();
  // One invalid script.
  script_list->add_scripts();

  std::string proto_str;
  proto.SerializeToString(&proto_str);

  bool should_update_scripts = false;
  std::vector<std::unique_ptr<Script>> scripts;
  std::vector<std::unique_ptr<Action>> unused_actions;

  EXPECT_TRUE(ProtocolUtils::ParseActions(
      proto_str, /* global_payload= */ nullptr, /* script_payload= */ nullptr,
      &unused_actions, &scripts, &should_update_scripts));
  EXPECT_TRUE(should_update_scripts);
  EXPECT_THAT(scripts, SizeIs(1));
  EXPECT_THAT("a", Eq(scripts[0]->handle.path));
  EXPECT_THAT("name", Eq(scripts[0]->handle.name));
}

}  // namespace
}  // namespace autofill_assistant
