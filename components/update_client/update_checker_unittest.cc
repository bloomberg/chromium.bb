// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/version.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/update_checker.h"
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

}  // namespace

class UpdateCheckerTest : public testing::Test {
 public:
  UpdateCheckerTest();
  ~UpdateCheckerTest() override;

  // Overrides from testing::Test.
  void SetUp() override;
  void TearDown() override;

  void UpdateCheckComplete(const GURL& original_url,
                           int error,
                           const std::string& error_message,
                           const UpdateResponse::Results& results);

 protected:
  void Quit();
  void RunThreads();
  void RunThreadsUntilIdle();

  CrxUpdateItem BuildCrxUpdateItem();

  scoped_refptr<TestConfigurator> config_;

  scoped_ptr<UpdateChecker> update_checker_;

  scoped_ptr<InterceptorFactory> interceptor_factory_;
  URLRequestPostInterceptor* post_interceptor_;  // Owned by the factory.

  GURL original_url_;
  int error_;
  std::string error_message_;
  UpdateResponse::Results results_;

 private:
  base::MessageLoopForIO loop_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(UpdateCheckerTest);
};

UpdateCheckerTest::UpdateCheckerTest() : post_interceptor_(NULL), error_(0) {
}

UpdateCheckerTest::~UpdateCheckerTest() {
}

void UpdateCheckerTest::SetUp() {
  config_ = new TestConfigurator(base::ThreadTaskRunnerHandle::Get(),
                                 base::ThreadTaskRunnerHandle::Get());
  interceptor_factory_.reset(
      new InterceptorFactory(base::ThreadTaskRunnerHandle::Get()));
  post_interceptor_ = interceptor_factory_->CreateInterceptor();
  EXPECT_TRUE(post_interceptor_);

  update_checker_.reset();

  error_ = 0;
  error_message_.clear();
  results_ = UpdateResponse::Results();
}

void UpdateCheckerTest::TearDown() {
  update_checker_.reset();

  post_interceptor_ = NULL;
  interceptor_factory_.reset();

  config_ = nullptr;

  // The PostInterceptor requires the message loop to run to destruct correctly.
  // TODO(sorin): This is fragile and should be fixed.
  RunThreadsUntilIdle();
}

void UpdateCheckerTest::RunThreads() {
  base::RunLoop runloop;
  quit_closure_ = runloop.QuitClosure();
  runloop.Run();

  // Since some tests need to drain currently enqueued tasks such as network
  // intercepts on the IO thread, run the threads until they are
  // idle. The component updater service won't loop again until the loop count
  // is set and the service is started.
  RunThreadsUntilIdle();
}

void UpdateCheckerTest::RunThreadsUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void UpdateCheckerTest::Quit() {
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

void UpdateCheckerTest::UpdateCheckComplete(
    const GURL& original_url,
    int error,
    const std::string& error_message,
    const UpdateResponse::Results& results) {
  original_url_ = original_url;
  error_ = error;
  error_message_ = error_message;
  results_ = results;
  Quit();
}

CrxUpdateItem UpdateCheckerTest::BuildCrxUpdateItem() {
  CrxComponent crx_component;
  crx_component.name = "test_jebg";
  crx_component.pk_hash.assign(jebg_hash, jebg_hash + arraysize(jebg_hash));
  crx_component.installer = NULL;
  crx_component.version = base::Version("0.9");
  crx_component.fingerprint = "fp1";

  CrxUpdateItem crx_update_item;
  crx_update_item.state = CrxUpdateItem::State::kNew;
  crx_update_item.id = "jebgalgnebhfojomionfpkfelancnnkf";
  crx_update_item.component = crx_component;

  return crx_update_item;
}

TEST_F(UpdateCheckerTest, UpdateCheckSuccess) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_1.xml")));

  update_checker_ = UpdateChecker::Create(config_);

  CrxUpdateItem item(BuildCrxUpdateItem());
  std::vector<CrxUpdateItem*> items_to_check;
  items_to_check.push_back(&item);

  update_checker_->CheckForUpdates(
      items_to_check, "extra=\"params\"",
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));

  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // Sanity check the request.
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
                              "request protocol=\"3.0\" extra=\"params\""));
  // The request must not contain any "dlpref" in the default case.
  EXPECT_EQ(string::npos,
            post_interceptor_->GetRequests()[0].find(" dlpref=\""));
  EXPECT_NE(
      string::npos,
      post_interceptor_->GetRequests()[0].find(
          "app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" version=\"0.9\">"
          "<updatecheck /><packages><package fp=\"fp1\"/></packages></app>"));

  EXPECT_NE(string::npos,
            post_interceptor_->GetRequests()[0].find("<hw physmemory="));

  // Sanity check the arguments of the callback after parsing.
  ASSERT_FALSE(config_->UpdateUrl().empty());
  EXPECT_EQ(config_->UpdateUrl().front(), original_url_);
  EXPECT_EQ(0, error_);
  EXPECT_TRUE(error_message_.empty());
  EXPECT_EQ(1ul, results_.list.size());
  EXPECT_STREQ("jebgalgnebhfojomionfpkfelancnnkf",
               results_.list[0].extension_id.c_str());
  EXPECT_STREQ("1.0", results_.list[0].manifest.version.c_str());
}

// Simulates a 403 server response error.
TEST_F(UpdateCheckerTest, UpdateCheckError) {
  EXPECT_TRUE(
      post_interceptor_->ExpectRequest(new PartialMatch("updatecheck"), 403));

  update_checker_ = UpdateChecker::Create(config_);

  CrxUpdateItem item(BuildCrxUpdateItem());
  std::vector<CrxUpdateItem*> items_to_check;
  items_to_check.push_back(&item);

  update_checker_->CheckForUpdates(
      items_to_check, "", base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                                     base::Unretained(this)));

  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  ASSERT_FALSE(config_->UpdateUrl().empty());
  EXPECT_EQ(config_->UpdateUrl().front(), original_url_);
  EXPECT_EQ(403, error_);
  EXPECT_STREQ("network error", error_message_.c_str());
  EXPECT_EQ(0ul, results_.list.size());
}

TEST_F(UpdateCheckerTest, UpdateCheckDownloadPreference) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_1.xml")));

  config_->SetDownloadPreference(string("cacheable"));

  update_checker_ = UpdateChecker::Create(config_);

  CrxUpdateItem item(BuildCrxUpdateItem());
  std::vector<CrxUpdateItem*> items_to_check;
  items_to_check.push_back(&item);

  update_checker_->CheckForUpdates(
      items_to_check, "extra=\"params\"",
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));

  RunThreads();

  // The request must contain dlpref="cacheable".
  EXPECT_NE(string::npos,
            post_interceptor_->GetRequests()[0].find(" dlpref=\"cacheable\""));
}

}  // namespace update_client
