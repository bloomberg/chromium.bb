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
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::SizeIs;
using ::testing::StrEq;

namespace {

class FakeClient : public Client {
 public:
  FakeClient(std::unique_ptr<UiController> ui_controller)
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
        std::move(web_controller), std::move(service));

    // Fetching scripts succeeds for all URLs, but return nothing.
    ON_CALL(*mock_service_, OnGetScriptsForUrl(_, _))
        .WillByDefault(RunOnceCallback<1>(true, ""));

    // Scripts run, but have no actions.
    ON_CALL(*mock_service_, OnGetActions(_, _))
        .WillByDefault(RunOnceCallback<1>(true, ""));

    // Element "exists" exists.
    ON_CALL(*mock_web_controller_, OnElementExists(ElementsAre("exists"), _))
        .WillByDefault(RunOnceCallback<1>(true));

    tester_ = content::WebContentsTester::For(web_contents());
  }

  void TearDown() override {
    controller_->OnDestroy();  // deletes the controller and mocks
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  static void AddRunnableScript(SupportsScriptResponseProto* response,
                                const std::string& name,
                                const std::string& path) {
    SupportedScriptProto* script = response->add_scripts();
    script->set_path(path);
    script->mutable_presentation()->set_name(name);
    script->mutable_presentation()
        ->mutable_precondition()
        ->add_elements_exist()
        ->add_selectors("exists");
  }

  // Updates the current url of the controller and forces a refresh, without
  // bothering with actually rendering any page content.
  void SimulateNavigateToUrl(const GURL& url) {
    tester_->SetLastCommittedURL(url);
    controller_->DidFinishLoad(nullptr, url);
  }

  // Sets up the next call to the service for scripts to return |response|.
  void SetNextScriptResponse(const SupportsScriptResponseProto& response) {
    std::string response_str;
    response.SerializeToString(&response_str);

    EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(_, _))
        .WillOnce(RunOnceCallback<1>(true, response_str));
  }

  UiDelegate* GetUiDelegate() { return controller_; }

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
  AddRunnableScript(&script_response, "script1 name", "script1");
  AddRunnableScript(&script_response, "script2 name", "script2");
  SetNextScriptResponse(script_response);

  // 2. checking the scripts
  // 3. offering the choices: script1 and script2
  // 4. script1 is chosen
  EXPECT_CALL(*mock_ui_controller_, UpdateScripts(SizeIs(2)))
      .WillOnce([this](const std::vector<ScriptHandle>& scripts) {
        EXPECT_EQ("script1", scripts[0].path);
        EXPECT_EQ("script2", scripts[1].path);
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

}  // namespace autofill_assistant
