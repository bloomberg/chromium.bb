// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_precondition.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "components/autofill_assistant/browser/mock_run_once_callback.h"
#include "components/autofill_assistant/browser/mock_web_controller.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;

// A callback that expects to be called immediately.
//
// This relies on ScriptPrecondition and WebController calling the callback
// immediately, which is not true in general, but is in this test.
class DirectCallback {
 public:
  DirectCallback() : was_run_(false), result_(false) {}

  // Returns a base::OnceCallback. The current instance must exist until
  // GetResultOrDie is called.
  base::OnceCallback<void(bool)> Get() {
    return base::BindOnce(&DirectCallback::Run, base::Unretained(this));
  }

  bool GetResultOrDie() {
    CHECK(was_run_);
    return result_;
  }

 private:
  void Run(bool result) {
    was_run_ = true;
    result_ = result;
  }

  bool was_run_;
  bool result_;
};

class ScriptPreconditionTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_web_controller_, OnElementExists(ElementsAre("exists"), _))
        .WillByDefault(RunOnceCallback<1>(true));
    ON_CALL(mock_web_controller_,
            OnElementExists(ElementsAre("does_not_exist"), _))
        .WillByDefault(RunOnceCallback<1>(false));

    SetUrl("http://www.example.com/path");
    ON_CALL(mock_web_controller_, GetUrl())
        .WillByDefault(Invoke(this, &ScriptPreconditionTest::GetUrl));
  }

 protected:
  // Implements WebController::GetUrl
  const GURL& GetUrl() { return url_; }

  void SetUrl(const std::string& url) { url_ = GURL(url); }

  // Runs the preconditions and returns the result.
  bool Check(const ScriptPreconditionProto& proto) {
    auto precondition = ScriptPrecondition::FromProto(proto);
    if (!precondition)
      return false;

    DirectCallback callback;
    precondition->Check(&mock_web_controller_, parameters_, callback.Get());
    return callback.GetResultOrDie();
  }

  GURL url_;
  MockWebController mock_web_controller_;
  std::map<std::string, std::string> parameters_;
};

TEST_F(ScriptPreconditionTest, NoConditions) {
  EXPECT_TRUE(Check(ScriptPreconditionProto::default_instance()));
}

TEST_F(ScriptPreconditionTest, DomainMatch) {
  ScriptPreconditionProto proto;
  proto.add_domain("match.example.com");
  proto.add_domain("alsomatch.example.com");

  SetUrl("http://match.example.com/path");
  EXPECT_TRUE(Check(proto));

  SetUrl("http://nomatch.example.com/path");
  EXPECT_FALSE(Check(proto));

  SetUrl("http://alsomatch.example.com/path");
  EXPECT_TRUE(Check(proto));
}

TEST_F(ScriptPreconditionTest, PathFullMatch) {
  ScriptPreconditionProto proto;
  proto.add_path_pattern("/match.*");
  proto.add_path_pattern("/alsomatch");

  SetUrl("http://www.example.com/match1");
  EXPECT_TRUE(Check(proto));

  SetUrl("http://www.example.com/match123");
  EXPECT_TRUE(Check(proto));

  SetUrl("http://www.example.com/doesnotmatch");
  EXPECT_FALSE(Check(proto));

  SetUrl("http://www.example.com/alsomatch");
  EXPECT_TRUE(Check(proto));
}

TEST_F(ScriptPreconditionTest, PathPartialMatch) {
  ScriptPreconditionProto proto;
  proto.add_path_pattern("/match");

  SetUrl("http://www.example.com/prefix/match/suffix");
  EXPECT_TRUE(Check(proto));
}

TEST_F(ScriptPreconditionTest, BadPathPattern) {
  ScriptPreconditionProto proto;
  proto.add_path_pattern("invalid[");

  EXPECT_EQ(nullptr, ScriptPrecondition::FromProto(proto));
}

TEST_F(ScriptPreconditionTest, ParameterMustExist) {
  ScriptPreconditionProto proto;
  ScriptParameterMatchProto* match = proto.add_script_parameter_match();
  match->set_name("param");
  match->set_exists(true);

  EXPECT_FALSE(Check(proto));

  parameters_["param"] = "exists";

  EXPECT_TRUE(Check(proto));
}

TEST_F(ScriptPreconditionTest, ParameterMustNotExist) {
  ScriptPreconditionProto proto;
  ScriptParameterMatchProto* match = proto.add_script_parameter_match();
  match->set_name("param");
  match->set_exists(false);

  EXPECT_TRUE(Check(proto));

  parameters_["param"] = "exists";

  EXPECT_FALSE(Check(proto));
}

TEST_F(ScriptPreconditionTest, ParameterMustHaveValue) {
  ScriptPreconditionProto proto;
  ScriptParameterMatchProto* match = proto.add_script_parameter_match();
  match->set_name("param");
  match->set_value_equals("value");

  EXPECT_FALSE(Check(proto));

  parameters_["param"] = "another value";

  EXPECT_FALSE(Check(proto));

  parameters_["param"] = "value";
  EXPECT_TRUE(Check(proto));
}

TEST_F(ScriptPreconditionTest, MultipleConditions) {
  ScriptPreconditionProto proto;
  proto.add_domain("match.example.com");
  proto.add_path_pattern("/path");
  proto.add_elements_exist()->add_selectors("exists");

  // Domain and path don't match.
  EXPECT_FALSE(Check(proto));

  // Domain, path and selector match.
  SetUrl("http://match.example.com/path");
  EXPECT_TRUE(Check(proto));

  // Selector doesn't match.
  proto.mutable_elements_exist(0)->set_selectors(0, "does_not_exist");
  EXPECT_FALSE(Check(proto));
}

}  // namespace
}  // namespace autofill_assistant
