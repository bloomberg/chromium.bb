// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_checker.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/component.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/url_request_post_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using std::string;

namespace update_client {

namespace {

base::FilePath test_file(const char* file) {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("update_client")
      .AppendASCII(file);
}

const char kUpdateItemId[] = "jebgalgnebhfojomionfpkfelancnnkf";

}  // namespace

class UpdateCheckerTest : public testing::Test {
 public:
  UpdateCheckerTest();
  ~UpdateCheckerTest() override;

  // Overrides from testing::Test.
  void SetUp() override;
  void TearDown() override;

  void UpdateCheckComplete(int error, int retry_after_sec);

 protected:
  void Quit();
  void RunThreads();

  std::unique_ptr<Component> MakeComponent() const;

  scoped_refptr<TestConfigurator> config_;
  std::unique_ptr<TestingPrefServiceSimple> pref_;
  std::unique_ptr<PersistedData> metadata_;

  std::unique_ptr<UpdateChecker> update_checker_;

  std::unique_ptr<InterceptorFactory> interceptor_factory_;
  URLRequestPostInterceptor* post_interceptor_ =
      nullptr;  // Owned by the factory.

  int error_ = 0;
  int retry_after_sec_ = 0;

  std::unique_ptr<UpdateContext> update_context_;

 private:
  std::unique_ptr<UpdateContext> MakeFakeUpdateContext() const;

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(UpdateCheckerTest);
};

UpdateCheckerTest::UpdateCheckerTest()
    : scoped_task_environment_(
          base::test::ScopedTaskEnvironment::MainThreadType::IO) {}

UpdateCheckerTest::~UpdateCheckerTest() {
}

void UpdateCheckerTest::SetUp() {
  config_ = base::MakeRefCounted<TestConfigurator>();
  pref_ = base::MakeUnique<TestingPrefServiceSimple>();
  PersistedData::RegisterPrefs(pref_->registry());
  metadata_ = base::MakeUnique<PersistedData>(pref_.get(), nullptr);
  interceptor_factory_ =
      base::MakeUnique<InterceptorFactory>(base::ThreadTaskRunnerHandle::Get());
  post_interceptor_ = interceptor_factory_->CreateInterceptor();
  EXPECT_TRUE(post_interceptor_);

  update_checker_ = nullptr;

  error_ = 0;
  retry_after_sec_ = 0;
  update_context_ = MakeFakeUpdateContext();
}

void UpdateCheckerTest::TearDown() {
  update_checker_ = nullptr;

  post_interceptor_ = nullptr;
  interceptor_factory_ = nullptr;

  config_ = nullptr;

  // The PostInterceptor requires the message loop to run to destruct correctly.
  // TODO(sorin): This is fragile and should be fixed.
  scoped_task_environment_.RunUntilIdle();
}

void UpdateCheckerTest::RunThreads() {
  base::RunLoop runloop;
  quit_closure_ = runloop.QuitClosure();
  runloop.Run();

  // Since some tests need to drain currently enqueued tasks such as network
  // intercepts on the IO thread, run the threads until they are
  // idle. The component updater service won't loop again until the loop count
  // is set and the service is started.
  scoped_task_environment_.RunUntilIdle();
}

void UpdateCheckerTest::Quit() {
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

void UpdateCheckerTest::UpdateCheckComplete(int error, int retry_after_sec) {
  error_ = error;
  retry_after_sec_ = retry_after_sec;
  Quit();
}

std::unique_ptr<UpdateContext> UpdateCheckerTest::MakeFakeUpdateContext()
    const {
  return base::MakeUnique<UpdateContext>(
      config_, false, std::vector<std::string>(),
      UpdateClient::CrxDataCallback(), UpdateEngine::NotifyObserversCallback(),
      UpdateEngine::Callback(), nullptr);
}

std::unique_ptr<Component> UpdateCheckerTest::MakeComponent() const {
  CrxComponent crx_component;
  crx_component.name = "test_jebg";
  crx_component.pk_hash.assign(jebg_hash, jebg_hash + arraysize(jebg_hash));
  crx_component.installer = NULL;
  crx_component.version = base::Version("0.9");
  crx_component.fingerprint = "fp1";

  auto component = base::MakeUnique<Component>(*update_context_, kUpdateItemId);
  component->state_ = base::MakeUnique<Component::StateNew>(component.get());
  component->crx_component_ = crx_component;

  return component;
}

TEST_F(UpdateCheckerTest, UpdateCheckSuccess) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_1.xml")));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  auto& component = components[kUpdateItemId];
  component->crx_component_.installer_attributes["ap"] = "some_ap";

  update_checker_->CheckForUpdates(
      std::vector<std::string>{kUpdateItemId}, components, "extra=\"params\"",
      true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // Sanity check the request.
  const auto request = post_interceptor_->GetRequests()[0];
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
                              "request protocol=\"3.1\" extra=\"params\""));
  // The request must not contain any "dlpref" in the default case.
  EXPECT_EQ(string::npos, request.find(" dlpref=\""));
  EXPECT_NE(
      string::npos,
      request.find(
          std::string("<app appid=\"") + kUpdateItemId +
          "\" version=\"0.9\" "
          "brand=\"TEST\" ap=\"some_ap\"><updatecheck/><ping rd=\"-2\" "));
  EXPECT_NE(string::npos,
            request.find("<packages><package fp=\"fp1\"/></packages></app>"));

  EXPECT_NE(string::npos, request.find("<hw physmemory="));

  // Tests that the progid is injected correctly from the configurator.
  EXPECT_NE(
      string::npos,
      request.find(" version=\"fake_prodid-30.0\" prodversion=\"30.0\" "));

  // Sanity check the arguments of the callback after parsing.
  EXPECT_EQ(0, error_);

  EXPECT_EQ(base::Version("1.0"), component->next_version_);
  EXPECT_EQ(1u, component->crx_urls_.size());
  EXPECT_EQ(
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx"),
      component->crx_urls_.front());

  EXPECT_STREQ("this", component->action_run_.c_str());

#if (OS_WIN)
  EXPECT_NE(string::npos, request.find(" domainjoined="));
#if defined(GOOGLE_CHROME_BUILD)
  // Check the Omaha updater state data in the request.
  EXPECT_NE(string::npos, request.find("<updater "));
  EXPECT_NE(string::npos, request.find(" name=\"Omaha\" "));
#endif  // GOOGLE_CHROME_BUILD
#endif  // OS_WINDOWS
}

// Tests that an invalid "ap" is not serialized.
TEST_F(UpdateCheckerTest, UpdateCheckInvalidAp) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_1.xml")));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  // Make "ap" too long.
  auto& component = components[kUpdateItemId];
  component->crx_component_.installer_attributes["ap"] = std::string(257, 'a');

  update_checker_->CheckForUpdates(
      std::vector<std::string>{kUpdateItemId}, components, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));

  RunThreads();

  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
                              std::string("app appid=\"") + kUpdateItemId +
                              "\" version=\"0.9\" "
                              "brand=\"TEST\"><updatecheck/><ping rd=\"-2\" "));
  EXPECT_NE(string::npos,
            post_interceptor_->GetRequests()[0].find(
                "<packages><package fp=\"fp1\"/></packages></app>"));
}

TEST_F(UpdateCheckerTest, UpdateCheckSuccessNoBrand) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_1.xml")));

  config_->SetBrand("TOOLONG");   // Sets an invalid brand code.
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      std::vector<std::string>{kUpdateItemId}, components, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));

  RunThreads();

  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
                              std::string("<app appid=\"") + kUpdateItemId +
                              "\" version=\"0.9\">"
                              "<updatecheck/><ping rd=\"-2\" "));
  EXPECT_NE(string::npos,
            post_interceptor_->GetRequests()[0].find(
                "<packages><package fp=\"fp1\"/></packages></app>"));
}

// Simulates a 403 server response error.
TEST_F(UpdateCheckerTest, UpdateCheckError) {
  EXPECT_TRUE(
      post_interceptor_->ExpectRequest(new PartialMatch("updatecheck"), 403));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  auto& component = components[kUpdateItemId];

  update_checker_->CheckForUpdates(
      std::vector<std::string>{kUpdateItemId}, components, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  EXPECT_EQ(403, error_);
  EXPECT_FALSE(component->next_version_.IsValid());
}

TEST_F(UpdateCheckerTest, UpdateCheckDownloadPreference) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_1.xml")));

  config_->SetDownloadPreference(string("cacheable"));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  update_checker_->CheckForUpdates(
      std::vector<std::string>{kUpdateItemId}, components, "extra=\"params\"",
      true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));

  RunThreads();

  // The request must contain dlpref="cacheable".
  EXPECT_NE(string::npos,
            post_interceptor_->GetRequests()[0].find(" dlpref=\"cacheable\""));
}

// This test is checking that an update check signed with CUP fails, since there
// is currently no entity that can respond with a valid signed response.
// A proper CUP test requires network mocks, which are not available now.
TEST_F(UpdateCheckerTest, UpdateCheckCupError) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_1.xml")));

  config_->SetEnabledCupSigning(true);
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  const auto& component = components[kUpdateItemId];

  update_checker_->CheckForUpdates(
      std::vector<std::string>{kUpdateItemId}, components, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));

  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // Sanity check the request.
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
                              std::string("<app appid=\"") + kUpdateItemId +
                              "\" version=\"0.9\" "
                              "brand=\"TEST\"><updatecheck/><ping rd=\"-2\" "));
  EXPECT_NE(string::npos,
            post_interceptor_->GetRequests()[0].find(
                "<packages><package fp=\"fp1\"/></packages></app>"));

  // Expect an error since the response is not trusted.
  EXPECT_EQ(-10000, error_);
  EXPECT_FALSE(component->next_version_.IsValid());
}

// Tests that the UpdateCheckers will not make an update check for a
// component that requires encryption when the update check URL is unsecure.
TEST_F(UpdateCheckerTest, UpdateCheckRequiresEncryptionError) {
  config_->SetUpdateCheckUrl(GURL("http:\\foo\bar"));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  auto& component = components[kUpdateItemId];
  component->crx_component_.requires_network_encryption = true;

  update_checker_->CheckForUpdates(
      std::vector<std::string>{kUpdateItemId}, components, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(-1, error_);
  EXPECT_FALSE(component->next_version_.IsValid());
}

// Tests that the PersistedData will get correctly update and reserialize
// the elapsed_days value.
TEST_F(UpdateCheckerTest, UpdateCheckDateLastRollCall) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_4.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_4.xml")));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  // Do two update-checks.
  update_checker_->CheckForUpdates(
      std::vector<std::string>{kUpdateItemId}, components, "extra=\"params\"",
      true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  update_checker_->CheckForUpdates(
      std::vector<std::string>{kUpdateItemId}, components, "extra=\"params\"",
      true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(2, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(2, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
                              "<ping rd=\"-2\" ping_freshness="));
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[1].find(
                              "<ping rd=\"3383\" ping_freshness="));
}

TEST_F(UpdateCheckerTest, UpdateCheckUpdateDisabled) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_1.xml")));

  config_->SetBrand("");
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  auto& component = components[kUpdateItemId];

  // Tests the scenario where:
  //  * the component does not support group policies.
  //  * the component updates are disabled.
  // Expects the group policy to be ignored and the update check to not
  // include the "updatedisabled" attribute.
  EXPECT_FALSE(
      component->crx_component_.supports_group_policy_enable_component_updates);

  update_checker_->CheckForUpdates(
      std::vector<std::string>{kUpdateItemId}, components, "", false,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
                              std::string("<app appid=\"") + kUpdateItemId +
                              "\" version=\"0.9\">"
                              "<updatecheck/>"));

  // Tests the scenario where:
  //  * the component supports group policies.
  //  * the component updates are disabled.
  // Expects the update check to include the "updatedisabled" attribute.
  component->crx_component_.supports_group_policy_enable_component_updates =
      true;
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  update_checker_->CheckForUpdates(
      std::vector<std::string>{kUpdateItemId}, components, "", false,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[1].find(
                              std::string("<app appid=\"") + kUpdateItemId +
                              "\" version=\"0.9\">"
                              "<updatecheck updatedisabled=\"true\"/>"));

  // Tests the scenario where:
  //  * the component does not support group policies.
  //  * the component updates are enabled.
  // Expects the update check to not include the "updatedisabled" attribute.
  component->crx_component_.supports_group_policy_enable_component_updates =
      false;
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  update_checker_->CheckForUpdates(
      std::vector<std::string>{kUpdateItemId}, components, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[2].find(
                              std::string("<app appid=\"") + kUpdateItemId +
                              "\" version=\"0.9\">"
                              "<updatecheck/>"));

  // Tests the scenario where:
  //  * the component supports group policies.
  //  * the component updates are enabled.
  // Expects the update check to not include the "updatedisabled" attribute.
  component->crx_component_.supports_group_policy_enable_component_updates =
      true;
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  update_checker_->CheckForUpdates(
      std::vector<std::string>{kUpdateItemId}, components, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[3].find(
                              std::string("<app appid=\"") + kUpdateItemId +
                              "\" version=\"0.9\">"
                              "<updatecheck/>"));
}

TEST_F(UpdateCheckerTest, NoUpdateActionRun) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"),
      test_file("updatecheck_reply_noupdate.xml")));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  IdToComponentPtrMap components;
  components[kUpdateItemId] = MakeComponent();

  auto& component = components[kUpdateItemId];

  update_checker_->CheckForUpdates(
      std::vector<std::string>{kUpdateItemId}, components, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  EXPECT_EQ(0, error_);
  EXPECT_STREQ("this", component->action_run_.c_str());
}

}  // namespace update_client
