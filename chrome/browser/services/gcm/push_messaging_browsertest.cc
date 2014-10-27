// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/fake_gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/services/gcm/push_messaging_application_id.h"
#include "chrome/browser/services/gcm/push_messaging_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"

namespace gcm {

namespace {
// Responds to a confirm infobar by accepting or cancelling it. Responds to at
// most one infobar.
class InfoBarResponder : public infobars::InfoBarManager::Observer {
 public:
  InfoBarResponder(Browser* browser, bool accept)
      : infobar_service_(InfoBarService::FromWebContents(
            browser->tab_strip_model()->GetActiveWebContents())),
        accept_(accept),
        has_observed_(false) {
    infobar_service_->AddObserver(this);
  }

  virtual ~InfoBarResponder() { infobar_service_->RemoveObserver(this); }

  // infobars::InfoBarManager::Observer
  virtual void OnInfoBarAdded(infobars::InfoBar* infobar) override {
    if (has_observed_)
      return;
    has_observed_ = true;
    ConfirmInfoBarDelegate* delegate =
        infobar->delegate()->AsConfirmInfoBarDelegate();
    DCHECK(delegate);

    // Respond to the infobar asynchronously, like a person.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            &InfoBarResponder::Respond, base::Unretained(this), delegate));
  }

 private:
  void Respond(ConfirmInfoBarDelegate* delegate) {
    if (accept_) {
      delegate->Accept();
    } else {
      delegate->Cancel();
    }
  }

  InfoBarService* infobar_service_;
  bool accept_;
  bool has_observed_;
};
}  // namespace

class PushMessagingBrowserTest : public InProcessBrowserTest {
 public:
  PushMessagingBrowserTest() : gcm_service_(nullptr) {}
  virtual ~PushMessagingBrowserTest() {}

  // InProcessBrowserTest:
  virtual void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  // InProcessBrowserTest:
  virtual void SetUp() override {
    https_server_.reset(new net::SpawnedTestServer(
        net::SpawnedTestServer::TYPE_HTTPS,
        net::BaseTestServer::SSLOptions(
            net::BaseTestServer::SSLOptions::CERT_OK),
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data/"))));
    ASSERT_TRUE(https_server_->Start());

    InProcessBrowserTest::SetUp();
  }

  // InProcessBrowserTest:
  virtual void SetUpOnMainThread() override {
    gcm_service_ = static_cast<FakeGCMProfileService*>(
        GCMProfileServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(), &FakeGCMProfileService::Build));
    gcm_service_->set_collect(true);

    loadTestPage();

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void loadTestPage() {
    ui_test_utils::NavigateToURL(
        browser(), https_server_->GetURL("files/push_messaging/test.html"));
  }

  bool RunScript(const std::string& script, std::string* result) {
    return content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
        script,
        result);
  }

  net::SpawnedTestServer* https_server() const { return https_server_.get(); }

  FakeGCMProfileService* gcm_service() const { return gcm_service_; }

  PushMessagingServiceImpl* push_service() {
    return static_cast<PushMessagingServiceImpl*>(
        gcm_service_->push_messaging_service());
  }

 private:
  scoped_ptr<net::SpawnedTestServer> https_server_;
  FakeGCMProfileService* gcm_service_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, RegisterSuccess) {
  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  InfoBarResponder accepting_responder(browser(), true);

  ASSERT_TRUE(RunScript("registerPush()", &script_result));
  EXPECT_EQ(std::string(kPushMessagingEndpoint) + " - 1", script_result);

  PushMessagingApplicationId app_id(https_server()->GetURL(""), 0L);
  EXPECT_EQ(app_id.ToString(), gcm_service()->last_registered_app_id());
  EXPECT_EQ("1234567890", gcm_service()->last_registered_sender_ids()[0]);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, RegisterFailureNoPermission) {
  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  InfoBarResponder cancelling_responder(browser(), false);

  ASSERT_TRUE(RunScript("registerPush()", &script_result));
  EXPECT_EQ("AbortError - Registration failed - permission denied",
            script_result);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PushEventSuccess) {
  std::string script_result;

  ASSERT_TRUE(RunScript("registerServiceWorker()", &script_result));
  ASSERT_EQ("ok - service worker registered", script_result);

  InfoBarResponder accepting_responder(browser(), true);

  ASSERT_TRUE(RunScript("registerPush()", &script_result));
  EXPECT_EQ(std::string(kPushMessagingEndpoint) + " - 1", script_result);

  PushMessagingApplicationId app_id(https_server()->GetURL(""), 0L);
  EXPECT_EQ(app_id.ToString(), gcm_service()->last_registered_app_id());
  EXPECT_EQ("1234567890", gcm_service()->last_registered_sender_ids()[0]);

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("false - is not controlled", script_result);

  loadTestPage();  // Reload to become controlled.

  ASSERT_TRUE(RunScript("isControlled()", &script_result));
  ASSERT_EQ("true - is controlled", script_result);

  GCMClient::IncomingMessage message;
  GCMClient::MessageData messageData;
  messageData.insert(std::pair<std::string, std::string>("data", "testdata"));
  message.data = messageData;
  push_service()->OnMessage(app_id.ToString(), message);
  ASSERT_TRUE(RunScript("pushData.get()", &script_result));
  EXPECT_EQ("testdata", script_result);
}

}  // namespace gcm
