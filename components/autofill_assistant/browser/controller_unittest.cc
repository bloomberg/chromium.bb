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
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::ReturnRef;

namespace {

class FakeClient : public Client {
 public:
  explicit FakeClient(std::unique_ptr<UiController> ui_controller)
      : ui_controller_(std::move(ui_controller)) {}

  // Implements Client
  std::string GetApiKey() override { return ""; }

  UiController* GetUiController() override { return ui_controller_.get(); }

 private:
  std::unique_ptr<UiController> ui_controller_;
};

}  // namespace

class ControllerTest : public content::RenderViewHostTestHarness {
 public:
  ControllerTest() {}
  ~ControllerTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    auto ui_controller = std::make_unique<NiceMock<MockUiController>>();
    mock_ui_controller_ = ui_controller.get();
    auto web_controller = std::make_unique<NiceMock<MockWebController>>();
    mock_web_controller_ = web_controller.get();
    auto service = std::make_unique<NiceMock<MockService>>();
    mock_service_ = service.get();
    controller_ = new Controller(
        web_contents(), std::make_unique<FakeClient>(std::move(ui_controller)),
        std::move(web_controller), std::move(service),
        std::make_unique<std::map<std::string, std::string>>());

    // Fetching scripts succeeds for all URLs, but return nothing.
    ON_CALL(*mock_service_, OnGetScriptsForUrl(_, _))
        .WillByDefault(RunOnceCallback<1>(true, ""));

    // Scripts run, but have no actions.
    ON_CALL(*mock_service_, OnGetActions(_, _))
        .WillByDefault(RunOnceCallback<1>(true, ""));

    // Make WebController::GetUrl accessible.
    ON_CALL(*mock_web_controller_, GetUrl()).WillByDefault(ReturnRef(url_));

    tester_ = content::WebContentsTester::For(web_contents());
  }

  void TearDown() override {
    controller_->OnDestroy();  // deletes the controller and mocks
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

  // Updates the current url of the controller and forces a refresh, without
  // bothering with actually rendering any page content.
  void SimulateNavigateToUrl(const GURL& url) {
    tester_->SetLastCommittedURL(url);
    controller_->DidFinishLoad(nullptr, url);
  }

  void SimulateUserInteraction(const blink::WebInputEvent::Type type) {
    controller_->DidGetUserInteraction(type);
  }

  // Sets up the next call to the service for scripts to return |response|.
  void SetNextScriptResponse(const SupportsScriptResponseProto& response) {
    std::string response_str;
    response.SerializeToString(&response_str);

    EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(_, _))
        .WillOnce(RunOnceCallback<1>(true, response_str));
  }

  UiDelegate* GetUiDelegate() { return controller_; }

  GURL url_;
  MockService* mock_service_;
  MockWebController* mock_web_controller_;
  MockUiController* mock_ui_controller_;
  content::WebContentsTester* tester_;
  Controller* controller_;
};

TEST_F(ControllerTest, FetchAndRunScripts) {
  // Going to the URL triggers a whole flow:
  // 1. loading scripts
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script1");
  AddRunnableScript(&script_response, "script2");
  SetNextScriptResponse(script_response);

  // 2. checking the scripts
  // 3. offering the choices: script1 and script2
  // 4. script1 is chosen
  EXPECT_CALL(*mock_ui_controller_, UpdateScripts(SizeIs(2)))
      .WillOnce([this](const std::vector<ScriptHandle>& scripts) {
        std::vector<std::string> paths;
        for (const auto& script : scripts) {
          paths.emplace_back(script.path);
        }
        EXPECT_THAT(paths, UnorderedElementsAre("script1", "script2"));
        GetUiDelegate()->OnScriptSelected("script1");
      });

  // 5. script1 run successfully (no actions).
  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("script1"), _))
      .WillOnce(RunOnceCallback<1>(true, ""));

  // 6. offering the choice: script2
  EXPECT_CALL(*mock_ui_controller_, UpdateScripts(SizeIs(1)))
      .WillOnce([](const std::vector<ScriptHandle>& scripts) {
        EXPECT_EQ("script2", scripts[0].path);
      });
  // 7. As nothing is selected from the 2nd UpdateScripts call, the flow
  // terminates.

  // Start the flow.
  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
}

TEST_F(ControllerTest, RefreshScriptWhenDomainChanges) {
  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(Eq(GURL("http://a.example.com/path1")), _))
      .WillOnce(RunOnceCallback<1>(true, ""));
  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(Eq(GURL("http://b.example.com/path1")), _))
      .WillOnce(RunOnceCallback<1>(true, ""));

  SimulateNavigateToUrl(GURL("http://a.example.com/path1"));
  SimulateNavigateToUrl(GURL("http://a.example.com/path2"));
  SimulateNavigateToUrl(GURL("http://b.example.com/path1"));
  SimulateNavigateToUrl(GURL("http://b.example.com/path2"));
}

TEST_F(ControllerTest, Autostart) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable")
      ->mutable_presentation()
      ->set_autostart(true);
  SetNextScriptResponse(script_response);

  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("runnable"), _))
      .WillOnce(RunOnceCallback<1>(true, ""));
  EXPECT_CALL(*mock_ui_controller_, UpdateScripts(SizeIs(0)));

  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
}

TEST_F(ControllerTest, AutostartFallsBackToUpdateScriptAfterTap) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable")
      ->mutable_presentation()
      ->set_autostart(true);
  SetNextScriptResponse(script_response);

  EXPECT_CALL(*mock_ui_controller_, UpdateScripts(SizeIs(1)));
  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("runnable"), _)).Times(0);

  SimulateUserInteraction(blink::WebInputEvent::kGestureTap);
  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
}

TEST_F(ControllerTest, AutostartFallsBackToUpdateScriptAfterExecution) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable")
      ->mutable_presentation()
      ->set_autostart(true);
  SetNextScriptResponse(script_response);

  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("script1"), _));
  GetUiDelegate()->OnScriptSelected("script1");

  EXPECT_CALL(*mock_ui_controller_, UpdateScripts(SizeIs(1)));
  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("runnable"), _)).Times(0);

  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
}

}  // namespace autofill_assistant
