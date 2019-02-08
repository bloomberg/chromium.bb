// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/controller.h"

#include <memory>
#include <utility>

#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/mock_run_once_callback.h"
#include "components/autofill_assistant/browser/mock_service.h"
#include "components/autofill_assistant/browser/mock_ui_controller.h"
#include "components/autofill_assistant/browser/mock_web_controller.h"
#include "components/autofill_assistant/browser/service.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Sequence;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

namespace {

class FakeClient : public Client {
 public:
  explicit FakeClient(UiController* ui_controller)
      : ui_controller_(ui_controller) {}

  // Implements Client
  std::string GetApiKey() override { return ""; }
  AccessTokenFetcher* GetAccessTokenFetcher() override { return nullptr; }
  autofill::PersonalDataManager* GetPersonalDataManager() override {
    return nullptr;
  }
  std::string GetServerUrl() override { return ""; }
  UiController* GetUiController() override { return ui_controller_; }
  std::string GetAccountEmailAddress() override { return ""; }
  std::string GetLocale() override { return ""; }
  std::string GetCountryCode() override { return ""; }
  MOCK_METHOD1(Shutdown, void(Metrics::DropOutReason reason));
  MOCK_METHOD0(ShowUI, void());
  MOCK_METHOD0(DestroyUI, void());

 private:
  UiController* ui_controller_;
};

}  // namespace

class ControllerTest : public content::RenderViewHostTestHarness {
 public:
  ControllerTest() : fake_client_(&mock_ui_controller_) {}
  ~ControllerTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    auto web_controller = std::make_unique<NiceMock<MockWebController>>();
    mock_web_controller_ = web_controller.get();
    auto service = std::make_unique<NiceMock<MockService>>();
    mock_service_ = service.get();

    controller_ = std::make_unique<Controller>(web_contents(), &fake_client_);
    controller_->SetWebControllerAndServiceForTest(std::move(web_controller),
                                                   std::move(service));

    // Fetching scripts succeeds for all URLs, but return nothing.
    ON_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _))
        .WillByDefault(RunOnceCallback<2>(true, ""));

    // Scripts run, but have no actions.
    ON_CALL(*mock_service_, OnGetActions(_, _, _, _, _, _))
        .WillByDefault(RunOnceCallback<5>(true, ""));

    ON_CALL(*mock_service_, OnGetNextActions(_, _, _, _))
        .WillByDefault(RunOnceCallback<3>(true, ""));

    // Make WebController::GetUrl accessible.
    ON_CALL(*mock_web_controller_, GetUrl()).WillByDefault(ReturnRef(url_));

    ON_CALL(mock_ui_controller_, OnStateChanged(_))
        .WillByDefault(Invoke([this](AutofillAssistantState state) {
          states_.emplace_back(state);
        }));

    tester_ = content::WebContentsTester::For(web_contents());
  }

  void TearDown() override {
    // Controller must be deleted before the WebContents, owned by
    // RenderViewHostTestHarness. In production, this is guaranteed by
    // autofill_assistant::ClientAndroid, which owns Controller.
    controller_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  static SupportedScriptProto* AddRunnableScript(
      SupportsScriptResponseProto* response,
      const std::string& name_and_path) {
    SupportedScriptProto* script = response->add_scripts();
    script->set_path(name_and_path);
    script->mutable_presentation()->set_name(name_and_path);
    return script;
  }

  static void RunOnce(SupportedScriptProto* proto) {
    auto* run_once = proto->mutable_presentation()
                         ->mutable_precondition()
                         ->add_script_status_match();
    run_once->set_script(proto->path());
    run_once->set_status(SCRIPT_STATUS_NOT_RUN);
  }

  void SetupScriptsForURL(const std::string& url,
                          SupportsScriptResponseProto scripts) {
    std::string scripts_str;
    scripts.SerializeToString(&scripts_str);
    EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(Eq(GURL(url)), _, _))
        .WillOnce(RunOnceCallback<2>(true, scripts_str));
  }

  void Start() {
    GURL initialUrl("http://initialurl.com");
    controller_->Start(initialUrl, /* parameters= */ {});
  }

  void SetLastCommittedUrl(const GURL& url) {
    url_ = url;
    tester_->SetLastCommittedURL(url);
  }

  // Updates the current url of the controller and forces a refresh, without
  // bothering with actually rendering any page content.
  void SimulateNavigateToUrl(const GURL& url) {
    SetLastCommittedUrl(url);
    controller_->DidFinishLoad(nullptr, url);
  }

  void SimulateWebContentsFocused() {
    controller_->OnWebContentsFocused(nullptr);
  }

  void SimulateProgressChanged(double progress) {
    controller_->LoadProgressChanged(web_contents(), progress);
  }

  // Sets up the next call to the service for scripts to return |response|.
  void SetNextScriptResponse(const SupportsScriptResponseProto& response) {
    std::string response_str;
    response.SerializeToString(&response_str);

    EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _))
        .WillOnce(RunOnceCallback<2>(true, response_str));
  }

  // Sets up all calls to the service for scripts to return |response|.
  void SetRepeatedScriptResponse(const SupportsScriptResponseProto& response) {
    std::string response_str;
    response.SerializeToString(&response_str);

    EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _))
        .WillRepeatedly(RunOnceCallback<2>(true, response_str));
  }

  void ExecuteScript(const std::string& script_path) {
    controller_->OnScriptSelected(script_path);
  }

  UiDelegate* GetUiDelegate() { return controller_.get(); }

  GURL url_;
  std::vector<AutofillAssistantState> states_;
  MockService* mock_service_;
  MockWebController* mock_web_controller_;
  NiceMock<FakeClient> fake_client_;
  NiceMock<MockUiController> mock_ui_controller_;
  content::WebContentsTester* tester_;

  std::unique_ptr<Controller> controller_;
};

TEST_F(ControllerTest, FetchAndRunScripts) {
  Start();

  // Going to the URL triggers a whole flow:
  // loading scripts
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script1");
  auto* script2 = AddRunnableScript(&script_response, "script2");
  RunOnce(script2);
  SetNextScriptResponse(script_response);

  testing::InSequence seq;

  // Start the flow.
  SimulateNavigateToUrl(GURL("http://a.example.com/path"));

  // Offering the choices: script1 and script2
  EXPECT_EQ(AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT,
            controller_->GetState());
  EXPECT_THAT(controller_->GetChips(),
              UnorderedElementsAre(Field(&Chip::text, StrEq("script1")),
                                   Field(&Chip::text, StrEq("script2"))));

  // Choose script2 and run it successfully.
  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("script2"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, ""));
  controller_->SelectChip(1);

  // Offering the remaining choice: script1 as script2 can only run once.
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());
  EXPECT_THAT(controller_->GetChips(),
              ElementsAre(Field(&Chip::text, StrEq("script1"))));
}

TEST_F(ControllerTest, ReportPromptAndChipsChanged) {
  Start();

  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script1");
  AddRunnableScript(&script_response, "script2");
  SetNextScriptResponse(script_response);

  EXPECT_CALL(mock_ui_controller_, OnChipsChanged(SizeIs(2)));
  EXPECT_CALL(
      mock_ui_controller_,
      OnStateChanged(AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT));
  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
}

TEST_F(ControllerTest, ClearChipsWhenRunning) {
  Start();

  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script1");
  AddRunnableScript(&script_response, "script2");
  SetNextScriptResponse(script_response);

  // Discover 2 scripts, one is selected and run (with no chips shown), then the
  // same chips are shown.
  {
    testing::InSequence seq;
    // Discover 2 scripts, script1 and script2.
    EXPECT_CALL(mock_ui_controller_, OnChipsChanged(SizeIs(2)));
    // Set of chips is cleared while running script1.
    EXPECT_CALL(mock_ui_controller_, OnChipsChanged(SizeIs(0)));
    // This test doesn't specify what happens after that.
    EXPECT_CALL(mock_ui_controller_, OnChipsChanged(_)).Times(AnyNumber());
  }
  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
  controller_->SelectChip(0);
}

TEST_F(ControllerTest, ShowFirstInitialStatusMessage) {
  Start();

  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script1");

  SupportedScriptProto* script2 =
      AddRunnableScript(&script_response, "script2");
  script2->mutable_presentation()->set_initial_prompt("script2 prompt");
  script2->mutable_presentation()->set_priority(10);

  SupportedScriptProto* script3 =
      AddRunnableScript(&script_response, "script3");
  script3->mutable_presentation()->set_initial_prompt("script3 prompt");
  script3->mutable_presentation()->set_priority(5);

  SupportedScriptProto* script4 =
      AddRunnableScript(&script_response, "script4");
  script4->mutable_presentation()->set_initial_prompt("script4 prompt");
  script4->mutable_presentation()->set_priority(8);

  SetNextScriptResponse(script_response);

  // Start the flow.
  SimulateNavigateToUrl(GURL("http://a.example.com/path"));

  // Script3, with higher priority (lower number), wins.
  EXPECT_EQ("script3 prompt", controller_->GetStatusMessage());
  EXPECT_THAT(controller_->GetChips(), SizeIs(4));
}

TEST_F(ControllerTest, Stop) {
  Start();

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_stop();
  std::string actions_response_str;
  actions_response.SerializeToString(&actions_response_str);
  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("stop"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, actions_response_str));

  testing::InSequence seq;
  EXPECT_CALL(fake_client_, Shutdown(Metrics::SCRIPT_SHUTDOWN));

  ExecuteScript("stop");

  // Simulates Client::Shutdown(SCRIPT_SHUTDOWN)
  EXPECT_CALL(mock_ui_controller_, WillShutdown(Metrics::SCRIPT_SHUTDOWN));
  EXPECT_TRUE(controller_->Terminate(Metrics::SCRIPT_SHUTDOWN));
}

TEST_F(ControllerTest, Reset) {
  Start();

    // 1. Fetch scripts for URL, which in contains a single "reset" script.
    SupportsScriptResponseProto script_response;
    auto* reset_script = AddRunnableScript(&script_response, "reset");
    RunOnce(reset_script);
    std::string script_response_str;
    script_response.SerializeToString(&script_response_str);
    EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _))
        .WillRepeatedly(RunOnceCallback<2>(true, script_response_str));

    SimulateNavigateToUrl(GURL("http://a.example.com/path"));
    EXPECT_THAT(controller_->GetChips(),
                ElementsAre(Field(&Chip::text, StrEq("reset"))));

    // 2. Execute the "reset" script, which contains a reset action.
    ActionsResponseProto actions_response;
    actions_response.add_actions()->mutable_reset();
    std::string actions_response_str;
    actions_response.SerializeToString(&actions_response_str);
    EXPECT_CALL(*mock_service_, OnGetActions(StrEq("reset"), _, _, _, _, _))
        .WillOnce(RunOnceCallback<5>(true, actions_response_str));

    controller_->GetClientMemory()->set_selected_card(
        std::make_unique<autofill::CreditCard>());
    EXPECT_TRUE(controller_->GetClientMemory()->has_selected_card());

    controller_->SelectChip(0);

    // Resetting should have cleared the client memory
    EXPECT_FALSE(controller_->GetClientMemory()->has_selected_card());

    // The reset script should be available again, even though it's marked
    // RunOnce, as the script state should have been cleared as well.
    EXPECT_THAT(controller_->GetChips(),
                ElementsAre(Field(&Chip::text, StrEq("reset"))));
}

TEST_F(ControllerTest, RefreshScriptWhenDomainChanges) {
  Start();

  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(Eq(GURL("http://a.example.com/path1")), _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));
  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(Eq(GURL("http://b.example.com/path1")), _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  SimulateNavigateToUrl(GURL("http://a.example.com/path1"));
  SimulateNavigateToUrl(GURL("http://a.example.com/path2"));
  SimulateNavigateToUrl(GURL("http://b.example.com/path1"));
  SimulateNavigateToUrl(GURL("http://b.example.com/path2"));
}

TEST_F(ControllerTest, ForwardParameters) {
  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(_, Contains(Pair("a", "b")), _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  GURL initialUrl("http://example.com/");
  std::map<std::string, std::string> parameters;
  parameters["a"] = "b";
  controller_->Start(initialUrl, parameters);
}

TEST_F(ControllerTest, Autostart) {
  Start();

  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable");
  AddRunnableScript(&script_response, "autostart")
      ->mutable_presentation()
      ->set_autostart(true);
  AddRunnableScript(&script_response, "alsorunnable");
  SetNextScriptResponse(script_response);

  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("autostart"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, ""));

  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
}

TEST_F(ControllerTest, AutostartFirstInterrupt) {
  Start();

  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable");

  auto* interrupt1 =
      AddRunnableScript(&script_response, "autostart interrupt 1");
  interrupt1->mutable_presentation()->set_interrupt(true);
  interrupt1->mutable_presentation()->set_priority(1);
  interrupt1->mutable_presentation()->set_autostart(true);

  auto* interrupt2 =
      AddRunnableScript(&script_response, "autostart interrupt 2");
  interrupt2->mutable_presentation()->set_interrupt(true);
  interrupt2->mutable_presentation()->set_priority(2);
  interrupt2->mutable_presentation()->set_autostart(true);

  SetNextScriptResponse(script_response);

  EXPECT_CALL(*mock_service_,
              OnGetActions(StrEq("autostart interrupt 1"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(false, ""));
  // The script fails, ending the flow. What matters is that the correct
  // expectation is met.

  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
}

TEST_F(ControllerTest, InterruptThenAutostart) {
  Start();

  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable");

  auto* interrupt = AddRunnableScript(&script_response, "autostart interrupt");
  interrupt->mutable_presentation()->set_interrupt(true);
  interrupt->mutable_presentation()->set_autostart(true);
  RunOnce(interrupt);

  auto* autostart = AddRunnableScript(&script_response, "autostart");
  autostart->mutable_presentation()->set_autostart(true);
  RunOnce(autostart);

  SetRepeatedScriptResponse(script_response);

  {
    testing::InSequence seq;
    EXPECT_CALL(*mock_service_,
                OnGetActions(StrEq("autostart interrupt"), _, _, _, _, _));
    EXPECT_CALL(*mock_service_,
                OnGetActions(StrEq("autostart"), _, _, _, _, _));
  }

  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
}

TEST_F(ControllerTest, AutostartIsNotPassedToTheUi) {
  Start();

  SupportsScriptResponseProto script_response;
  auto* autostart = AddRunnableScript(&script_response, "runnable");
  autostart->mutable_presentation()->set_autostart(true);
  RunOnce(autostart);
  SetRepeatedScriptResponse(script_response);

  EXPECT_CALL(mock_ui_controller_, OnChipsChanged(SizeIs(0u)))
      .Times(AnyNumber());
  EXPECT_CALL(mock_ui_controller_, OnChipsChanged(SizeIs(Gt(0u)))).Times(0);

  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
  EXPECT_THAT(controller_->GetChips(), SizeIs(0));
}

TEST_F(ControllerTest, LoadProgressChanged) {
  Start();

  SetLastCommittedUrl(GURL("http://a.example.com/path"));

  EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _)).Times(0);

  SimulateProgressChanged(0.1);
  SimulateProgressChanged(0.3);
  SimulateProgressChanged(0.5);

  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(Eq(GURL("http://a.example.com/path")), _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));
  SimulateProgressChanged(0.4);
}

TEST_F(ControllerTest, InitialUrlLoads) {
  GURL initialUrl("http://a.example.com/path");
  EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(Eq(initialUrl), _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  controller_->Start(initialUrl, /* parameters= */ {});
}

TEST_F(ControllerTest, CookieExperimentEnabled) {
  GURL initialUrl("http://a.example.com/path");

  // TODO(crbug.com/806868): Extend this test once the cookie information is
  // passed to the initial request. Currently the public controller API does not
  // yet allow proper testing.
  EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(Eq(initialUrl), _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  std::map<std::string, std::string> parameters;
  parameters.insert(std::make_pair("EXP_COOKIE", "1"));
  controller_->Start(initialUrl, parameters);

  // TODO(crbug.com): Make IsCookieExperimentEnabled private and remove this
  // test when we pass the cookie data along in the initial request so that it
  // can be tested.
  EXPECT_TRUE(controller_->IsCookieExperimentEnabled());
}

TEST_F(ControllerTest, ProgressIncreasesAtStart) {
  EXPECT_EQ(0, controller_->GetProgress());
  EXPECT_CALL(mock_ui_controller_, OnProgressChanged(10));
  Start();
  EXPECT_EQ(10, controller_->GetProgress());
}

TEST_F(ControllerTest, SetProgress) {
  Start();
  EXPECT_CALL(mock_ui_controller_, OnProgressChanged(20));
  controller_->SetProgress(20);
  EXPECT_EQ(20, controller_->GetProgress());
}

TEST_F(ControllerTest, IgnoreProgressDecreases) {
  Start();
  EXPECT_CALL(mock_ui_controller_, OnProgressChanged(Not(15)))
      .Times(AnyNumber());
  controller_->SetProgress(20);
  controller_->SetProgress(15);
  EXPECT_EQ(20, controller_->GetProgress());
}

TEST_F(ControllerTest, StateChanges) {
  EXPECT_EQ(AutofillAssistantState::INACTIVE, GetUiDelegate()->GetState());
  Start();
  EXPECT_EQ(AutofillAssistantState::STARTING, GetUiDelegate()->GetState());

  SupportsScriptResponseProto script_response;
  auto* script1 = AddRunnableScript(&script_response, "script1");
  RunOnce(script1);
  auto* script2 = AddRunnableScript(&script_response, "script2");
  RunOnce(script2);
  SetNextScriptResponse(script_response);

  SimulateNavigateToUrl(GURL("http://a.example.com/path"));

  EXPECT_EQ(AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT,
            GetUiDelegate()->GetState());

  // Run script1: State should become RUNNING, as there's another script, then
  // go back to prompt to propose that script.
  states_.clear();
  ExecuteScript("script1");  // returns immediately

  EXPECT_EQ(AutofillAssistantState::PROMPT, GetUiDelegate()->GetState());
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT));

  // Run script2: State should become STOPPED, as there are no more runnable
  // scripts.
  states_.clear();
  ExecuteScript("script2");

  EXPECT_EQ(AutofillAssistantState::STOPPED, GetUiDelegate()->GetState());
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT,
                                   AutofillAssistantState::STOPPED));
}

TEST_F(ControllerTest, ShowUIWhenStarting) {
  EXPECT_CALL(fake_client_, ShowUI());
  Start();
}

TEST_F(ControllerTest, ShowUIWhenContentsFocused) {
  SimulateWebContentsFocused();  // must not call ShowUI

  testing::InSequence seq;
  EXPECT_CALL(fake_client_, ShowUI());
  Start();  // must call ShowUI
  EXPECT_CALL(fake_client_, ShowUI());
  SimulateWebContentsFocused();  // must call ShowUI

  controller_->OnFatalError("test", Metrics::TAB_CHANGED);
  SimulateWebContentsFocused();  // must not call ShowUI
}

}  // namespace autofill_assistant
