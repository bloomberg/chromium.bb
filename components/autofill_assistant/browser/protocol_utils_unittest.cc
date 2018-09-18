// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/protocol_utils.h"

#include "base/macros.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::SizeIs;
using ::testing::IsEmpty;

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
  presentation->mutable_precondition()->add_domain("www.example.com");

  std::vector<std::unique_ptr<Script>> scripts;
  std::string proto_str;
  proto.SerializeToString(&proto_str);
  EXPECT_TRUE(ProtocolUtils::ParseScripts(proto_str, &scripts));
  ASSERT_THAT(scripts, SizeIs(1));
  EXPECT_EQ("path", scripts[0]->handle.path);
  EXPECT_EQ("name", scripts[0]->handle.name);
  EXPECT_TRUE(scripts[0]->handle.autostart);
  EXPECT_NE(nullptr, scripts[0]->precondition);
}

}  // namespace
}  // namespace autofill_assistant
