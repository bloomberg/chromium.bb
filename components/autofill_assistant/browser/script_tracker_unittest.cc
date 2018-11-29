// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_tracker.h"

#include <utility>

#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/mock_run_once_callback.h"
#include "components/autofill_assistant/browser/mock_service.h"
#include "components/autofill_assistant/browser/mock_ui_controller.h"
#include "components/autofill_assistant/browser/mock_web_controller.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::ReturnRef;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

class ScriptTrackerTest : public testing::Test,
                          public ScriptTracker::Listener,
                          public ScriptExecutorDelegate {
 public:
  void SetUp() override {
    ON_CALL(mock_web_controller_,
            OnElementCheck(kExistenceCheck, Eq(Selector({"exists"})), _))
        .WillByDefault(RunOnceCallback<2>(true));
    ON_CALL(
        mock_web_controller_,
        OnElementCheck(kExistenceCheck, Eq(Selector({"does_not_exist"})), _))
        .WillByDefault(RunOnceCallback<2>(false));
    ON_CALL(mock_web_controller_, GetUrl()).WillByDefault(ReturnRef(url_));

    // Scripts run, but have no actions.
    ON_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
        .WillByDefault(RunOnceCallback<5>(true, ""));
  }

 protected:
  ScriptTrackerTest()
      : no_runnable_scripts_anymore_(0),
        runnable_scripts_changed_(0),
        tracker_(this, this) {}

  // Overrides ScriptTrackerDelegate
  Service* GetService() override { return &mock_service_; }

  UiController* GetUiController() override { return &mock_ui_controller_; }

  WebController* GetWebController() override { return &mock_web_controller_; }

  ClientMemory* GetClientMemory() override { return &client_memory_; }

  const std::map<std::string, std::string>& GetParameters() override {
    return parameters_;
  }

  autofill::PersonalDataManager* GetPersonalDataManager() override {
    return nullptr;
  }

  content::WebContents* GetWebContents() override { return nullptr; }

  virtual void SetTouchableElementArea(
      const std::vector<Selector>& elements) override {}

  // Overrides ScriptTracker::Listener
  void OnRunnableScriptsChanged(
      const std::vector<ScriptHandle>& runnable_scripts) override {
    runnable_scripts_changed_++;
    runnable_scripts_ = runnable_scripts;
  }

  void OnNoRunnableScriptsAnymore() override { no_runnable_scripts_anymore_++; }

  void SetAndCheckScripts(const SupportsScriptResponseProto& response) {
    std::string response_str;
    response.SerializeToString(&response_str);
    std::vector<std::unique_ptr<Script>> scripts;
    ProtocolUtils::ParseScripts(response_str, &scripts);
    tracker_.SetScripts(std::move(scripts));
    tracker_.CheckScripts(base::TimeDelta::FromSeconds(0));
  }

  static SupportedScriptProto* AddScript(SupportsScriptResponseProto* response,
                                         const std::string& name,
                                         const std::string& path,
                                         const std::string& selector) {
    SupportedScriptProto* script = response->add_scripts();
    script->set_path(path);
    script->mutable_presentation()->set_name(name);
    if (!selector.empty()) {
      script->mutable_presentation()
          ->mutable_precondition()
          ->add_elements_exist()
          ->add_selectors(selector);
    }
    ScriptStatusMatchProto dont_run_twice_precondition;
    dont_run_twice_precondition.set_script(path);
    dont_run_twice_precondition.set_comparator(ScriptStatusMatchProto::EQUAL);
    dont_run_twice_precondition.set_status(SCRIPT_STATUS_NOT_RUN);
    *script->mutable_presentation()
         ->mutable_precondition()
         ->add_script_status_match() = dont_run_twice_precondition;
    return script;
  }

  const std::vector<ScriptHandle>& runnable_scripts() {
    return runnable_scripts_;
  }

  const std::vector<std::string> runnable_script_paths() {
    std::vector<std::string> paths;
    for (const auto& handle : runnable_scripts_) {
      paths.emplace_back(handle.path);
    }
    return paths;
  }

  std::string Serialize(const google::protobuf::MessageLite& message) {
    std::string output;
    message.SerializeToString(&output);
    return output;
  }

  GURL url_;
  NiceMock<MockService> mock_service_;
  NiceMock<MockWebController> mock_web_controller_;
  NiceMock<MockUiController> mock_ui_controller_;
  ClientMemory client_memory_;
  std::map<std::string, std::string> parameters_;

  // Number of times NoRunnableScriptsAnymore was called.
  int no_runnable_scripts_anymore_;
  // Number of times OnRunnableScriptsChanged was called.
  int runnable_scripts_changed_;
  std::vector<ScriptHandle> runnable_scripts_;
  ScriptTracker tracker_;
};

TEST_F(ScriptTrackerTest, NoScripts) {
  tracker_.SetScripts({});
  tracker_.CheckScripts(base::TimeDelta::FromSeconds(0));
  EXPECT_THAT(runnable_scripts(), IsEmpty());
  EXPECT_EQ(0, runnable_scripts_changed_);
  EXPECT_EQ(0, no_runnable_scripts_anymore_);
}

TEST_F(ScriptTrackerTest, SomeRunnableScripts) {
  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "not runnable name", "not runnable path",
            "does_not_exist");
  AddScript(&scripts, "runnable name", "runnable path", "exists");
  SetAndCheckScripts(scripts);

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("runnable name", runnable_scripts()[0].name);
  EXPECT_EQ("runnable path", runnable_scripts()[0].path);
}

TEST_F(ScriptTrackerTest, DoNotCheckInterruptWithNoName) {
  SupportsScriptResponseProto scripts;

  // The interrupt's preconditions would all be met, but it won't be reported
  // since it doesn't have a name.
  auto* no_name = AddScript(&scripts, "", "path1", "exists");
  no_name->mutable_presentation()->set_interrupt(true);

  // The interrupt's preconditions are met and it will be reported as a normal
  // script.
  auto* with_name = AddScript(&scripts, "with name", "path2", "exists");
  with_name->mutable_presentation()->set_interrupt(true);

  SetAndCheckScripts(scripts);

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("with name", runnable_scripts()[0].name);
}

TEST_F(ScriptTrackerTest, ReportInterruptToAutostart) {
  SupportsScriptResponseProto scripts;

  // The interrupt's preconditions are met and it will be reported as runnable
  // for autostart.
  auto* autostart = AddScript(&scripts, "", "path2", "exists");
  autostart->mutable_presentation()->set_interrupt(true);
  autostart->mutable_presentation()->set_autostart(true);
  SetAndCheckScripts(scripts);

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
}

TEST_F(ScriptTrackerTest, OrderScriptsByPriority) {
  SupportsScriptResponseProto scripts;

  SupportedScriptProto* a = scripts.add_scripts();
  a->set_path("a");
  a->mutable_presentation()->set_name("a");
  a->mutable_presentation()->set_priority(2);

  SupportedScriptProto* b = scripts.add_scripts();
  b->set_path("b");
  b->mutable_presentation()->set_name("b");
  b->mutable_presentation()->set_priority(3);

  SupportedScriptProto* c = scripts.add_scripts();
  c->set_path("c");
  c->mutable_presentation()->set_name("c");
  c->mutable_presentation()->set_priority(1);
  SetAndCheckScripts(scripts);

  ASSERT_THAT(runnable_script_paths(), ElementsAre("c", "a", "b"));
}

TEST_F(ScriptTrackerTest, NewScriptChangesNothing) {
  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "runnable name", "runnable path", "exists");
  SetAndCheckScripts(scripts);
  EXPECT_EQ(1, runnable_scripts_changed_);
  SetAndCheckScripts(scripts);
  EXPECT_EQ(1, runnable_scripts_changed_);
}

TEST_F(ScriptTrackerTest, NewScriptClearsRunnable) {
  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "runnable name", "runnable path", "exists");
  SetAndCheckScripts(scripts);
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  SetAndCheckScripts(SupportsScriptResponseProto::default_instance());
  EXPECT_EQ(2, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), IsEmpty());
}

TEST_F(ScriptTrackerTest, NewScriptAddsRunnable) {
  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "runnable name", "runnable path", "exists");
  SetAndCheckScripts(scripts);
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  AddScript(&scripts, "new runnable name", "new runnable path", "exists");
  SetAndCheckScripts(scripts);
  EXPECT_EQ(2, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(2));
}

TEST_F(ScriptTrackerTest, NewScriptChangesRunnable) {
  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "runnable name", "runnable path", "exists");
  SetAndCheckScripts(scripts);
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  scripts.clear_scripts();
  AddScript(&scripts, "new runnable name", "new runnable path", "exists");
  SetAndCheckScripts(scripts);
  EXPECT_EQ(2, runnable_scripts_changed_);
}

TEST_F(ScriptTrackerTest, CheckScriptsAgainAfterScriptEnd) {
  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "script 1", "script1", "exists");
  AddScript(&scripts, "script 2", "script2", "exists");
  SetAndCheckScripts(scripts);

  // Both scripts are runnable
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_script_paths(),
              UnorderedElementsAre("script1", "script2"));

  // run 'script 1'
  base::MockCallback<ScriptExecutor::RunScriptCallback> execute_callback;
  EXPECT_CALL(execute_callback,
              Run(Field(&ScriptExecutor::Result::success, true)));

  tracker_.ExecuteScript("script1", execute_callback.Get());
  tracker_.CheckScripts(base::TimeDelta::FromSeconds(0));

  // The 2nd time the scripts are checked, automatically after the script runs,
  // 'script1' isn't runnable anymore, because it's already been run.
  EXPECT_EQ(2, runnable_scripts_changed_);
  EXPECT_THAT(runnable_script_paths(), ElementsAre("script2"));
}

TEST_F(ScriptTrackerTest, CheckScriptsAfterDOMChange) {
  EXPECT_CALL(
      mock_web_controller_,
      OnElementCheck(kExistenceCheck, Eq(Selector({"maybe_exists"})), _))
      .WillOnce(RunOnceCallback<2>(false));

  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "script name", "script path", "maybe_exists");
  SetAndCheckScripts(scripts);

  // No scripts are runnable.
  EXPECT_THAT(runnable_scripts(), IsEmpty());

  // DOM has changed; OnElementExists now returns true.
  EXPECT_CALL(
      mock_web_controller_,
      OnElementCheck(kExistenceCheck, Eq(Selector({"maybe_exists"})), _))
      .WillOnce(RunOnceCallback<2>(true));
  tracker_.CheckScripts(base::TimeDelta::FromSeconds(0));

  // The script can now run
  ASSERT_THAT(runnable_script_paths(), ElementsAre("script path"));
}

TEST_F(ScriptTrackerTest, UpdateScriptList) {
  // 1. Initialize runnable scripts with a single valid script.
  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "runnable name", "runnable path", "exists");
  SetAndCheckScripts(scripts);

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("runnable name", runnable_scripts()[0].name);
  EXPECT_EQ("runnable path", runnable_scripts()[0].path);

  // 2. Run the action and trigger a script list update.
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("hi");

  *actions_response.mutable_update_script_list()->add_scripts() =
      *AddScript(&scripts, "update name", "update path", "exists");
  *actions_response.mutable_update_script_list()->add_scripts() =
      *AddScript(&scripts, "update name 2", "update path 2", "exists");

  EXPECT_CALL(mock_service_,
              OnGetActions(StrEq("runnable name"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(true, ""));

  base::MockCallback<ScriptExecutor::RunScriptCallback> execute_callback;
  EXPECT_CALL(execute_callback,
              Run(Field(&ScriptExecutor::Result::success, true)));
  tracker_.ExecuteScript("runnable name", execute_callback.Get());
  tracker_.CheckScripts(base::TimeDelta::FromSeconds(0));

  // 3. Verify that the runnable scripts have changed to the updated list.
  EXPECT_EQ(2, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(2));
  EXPECT_EQ("update name", runnable_scripts()[0].name);
  EXPECT_EQ("update path", runnable_scripts()[0].path);
  EXPECT_EQ("update name 2", runnable_scripts()[1].name);
  EXPECT_EQ("update path 2", runnable_scripts()[1].path);
}

TEST_F(ScriptTrackerTest, UpdateScriptListFromInterrupt) {
  // 1. Initialize runnable scripts with a single valid interrupt script.
  SupportsScriptResponseProto scripts;
  auto* script =
      AddScript(&scripts, "runnable name", "runnable path", "exists");
  script->mutable_presentation()->set_interrupt(true);
  SetAndCheckScripts(scripts);

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("runnable name", runnable_scripts()[0].name);
  EXPECT_EQ("runnable path", runnable_scripts()[0].path);

  // 2. Run the interrupt action and trigger a script list update from an
  // interrupt.
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("hi");

  *actions_response.mutable_update_script_list()->add_scripts() =
      *AddScript(&scripts, "update name", "update path", "exists");
  *actions_response.mutable_update_script_list()->add_scripts() =
      *AddScript(&scripts, "update name 2", "update path 2", "exists");

  EXPECT_CALL(mock_service_,
              OnGetActions(StrEq("runnable name"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(true, ""));

  base::MockCallback<ScriptExecutor::RunScriptCallback> execute_callback;
  EXPECT_CALL(execute_callback,
              Run(Field(&ScriptExecutor::Result::success, true)));
  tracker_.ExecuteScript("runnable name", execute_callback.Get());
  tracker_.CheckScripts(base::TimeDelta::FromSeconds(0));

  // 3. Verify that the runnable scripts have changed to the updated list.
  EXPECT_EQ(2, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(2));
  EXPECT_EQ("update name", runnable_scripts()[0].name);
  EXPECT_EQ("update path", runnable_scripts()[0].path);
  EXPECT_EQ("update name 2", runnable_scripts()[1].name);
  EXPECT_EQ("update path 2", runnable_scripts()[1].path);
}

}  // namespace autofill_assistant
