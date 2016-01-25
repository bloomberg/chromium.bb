// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/background_sync/background_sync_registration_handle.h"
#include "content/browser/background_sync/background_sync_status.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/background_sync_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/background_sync_test_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/network_change_notifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::NetworkChangeNotifier;

namespace content {

namespace {

const char kDefaultTestURL[] = "/background_sync/test.html";
const char kEmptyURL[] = "/background_sync/empty.html";
const char kRegisterSyncURL[] = "/background_sync/register_sync.html";

const char kSuccessfulOperationPrefix[] = "ok - ";

std::string BuildScriptString(const std::string& function,
                              const std::string& argument) {
  return base::StringPrintf("%s('%s');", function.c_str(), argument.c_str());
}

std::string BuildExpectedResult(const std::string& tag,
                                const std::string& action) {
  return base::StringPrintf("%s%s %s", kSuccessfulOperationPrefix, tag.c_str(),
                            action.c_str());
}

void OneShotPendingCallback(
    const base::Closure& quit,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    bool* result_out,
    bool result) {
  *result_out = result;
  task_runner->PostTask(FROM_HERE, quit);
}

void OneShotPendingDidGetSyncRegistration(
    const base::Callback<void(bool)>& callback,
    BackgroundSyncStatus error_type,
    scoped_ptr<BackgroundSyncRegistrationHandle> registration_handle) {
  ASSERT_EQ(BACKGROUND_SYNC_STATUS_OK, error_type);
  callback.Run(registration_handle->sync_state() ==
               BackgroundSyncState::PENDING);
}

void OneShotPendingDidGetSWRegistration(
    const scoped_refptr<BackgroundSyncContext> sync_context,
    const std::string& tag,
    const base::Callback<void(bool)>& callback,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  ASSERT_EQ(SERVICE_WORKER_OK, status);
  int64_t service_worker_id = registration->id();
  BackgroundSyncManager* sync_manager = sync_context->background_sync_manager();
  sync_manager->GetRegistration(
      service_worker_id, tag,
      base::Bind(&OneShotPendingDidGetSyncRegistration, callback));
}

void OneShotPendingOnIOThread(
    const scoped_refptr<BackgroundSyncContext> sync_context,
    const scoped_refptr<ServiceWorkerContextWrapper> sw_context,
    const std::string& tag,
    const GURL& url,
    const base::Callback<void(bool)>& callback) {
  sw_context->FindReadyRegistrationForDocument(
      url, base::Bind(&OneShotPendingDidGetSWRegistration, sync_context, tag,
                      callback));
}

void SetMaxSyncAttemptsOnIOThread(
    const scoped_refptr<BackgroundSyncContext>& sync_context,
    int max_sync_attempts) {
  BackgroundSyncManager* background_sync_manager =
      sync_context->background_sync_manager();
  background_sync_manager->SetMaxSyncAttemptsForTesting(max_sync_attempts);
}

}  // namespace

class BackgroundSyncBrowserTest : public ContentBrowserTest {
 public:
  BackgroundSyncBrowserTest() {}
  ~BackgroundSyncBrowserTest() override {}

  void SetUp() override {
    background_sync_test_util::SetIgnoreNetworkChangeNotifier(true);

    ContentBrowserTest::SetUp();
  }

  void SetIncognitoMode(bool incognito) {
    shell_ = incognito ? CreateOffTheRecordBrowser() : shell();
  }

  StoragePartition* GetStorage() {
    WebContents* web_contents = shell_->web_contents();
    return BrowserContext::GetStoragePartition(
        web_contents->GetBrowserContext(), web_contents->GetSiteInstance());
  }

  BackgroundSyncContext* GetSyncContext() {
    return GetStorage()->GetBackgroundSyncContext();
  }

  WebContents* web_contents() { return shell_->web_contents(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(jkarlin): Remove this once background sync is no longer
    // experimental.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server_->Start());

    SetIncognitoMode(false);
    SetMaxSyncAttempts(1);
    background_sync_test_util::SetOnline(web_contents(), true);
    ASSERT_TRUE(LoadTestPage(kDefaultTestURL));

    ContentBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override { https_server_.reset(); }

  bool LoadTestPage(const std::string& path) {
    return NavigateToURL(shell_, https_server_->GetURL(path));
  }

  bool RunScript(const std::string& script, std::string* result) {
    return content::ExecuteScriptAndExtractString(web_contents(), script,
                                                  result);
  }

  // Returns true if the one-shot sync with tag is currently pending. Fails
  // (assertion failure) if the tag isn't registered.
  bool OneShotPending(const std::string& tag);

  // Sets the BackgroundSyncManager's max sync attempts per registration.
  void SetMaxSyncAttempts(int max_sync_attempts);

  void ClearStoragePartitionData();

  std::string PopConsoleString();
  bool PopConsole(const std::string& expected_msg);
  bool RegisterServiceWorker();
  bool RegisterOneShot(const std::string& tag);
  bool RegisterOneShotFromServiceWorker(const std::string& tag);
  bool GetRegistrationOneShot(const std::string& tag);
  bool GetRegistrationOneShotFromServiceWorker(const std::string& tag);
  bool MatchRegistrations(const std::string& script_result,
                          const std::vector<std::string>& expected_tags);
  bool GetRegistrationsOneShot(const std::vector<std::string>& expected_tags);
  bool GetRegistrationsOneShotFromServiceWorker(
      const std::vector<std::string>& expected_tags);
  bool CompleteDelayedOneShot();
  bool RejectDelayedOneShot();

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  scoped_ptr<net::EmbeddedTestServer> https_server_;
  Shell* shell_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncBrowserTest);
};

bool BackgroundSyncBrowserTest::OneShotPending(const std::string& tag) {
  bool is_pending;
  base::RunLoop run_loop;

  StoragePartition* storage = GetStorage();
  BackgroundSyncContext* sync_context = storage->GetBackgroundSyncContext();
  ServiceWorkerContextWrapper* service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          storage->GetServiceWorkerContext());

  base::Callback<void(bool)> callback =
      base::Bind(&OneShotPendingCallback, run_loop.QuitClosure(),
                 base::ThreadTaskRunnerHandle::Get(), &is_pending);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&OneShotPendingOnIOThread, make_scoped_refptr(sync_context),
                 make_scoped_refptr(service_worker_context), tag,
                 https_server_->GetURL(kDefaultTestURL), callback));

  run_loop.Run();

  return is_pending;
}

void BackgroundSyncBrowserTest::SetMaxSyncAttempts(int max_sync_attempts) {
  base::RunLoop run_loop;

  StoragePartition* storage = GetStorage();
  BackgroundSyncContext* sync_context = storage->GetBackgroundSyncContext();

  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SetMaxSyncAttemptsOnIOThread,
                 make_scoped_refptr(sync_context), max_sync_attempts),
      run_loop.QuitClosure());

  run_loop.Run();
}

void BackgroundSyncBrowserTest::ClearStoragePartitionData() {
  // Clear data from the storage partition.  Parameters are set to clear data
  // for service workers, for all origins, for an unbounded time range.
  StoragePartition* storage = GetStorage();

  uint32_t storage_partition_mask =
      StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS;
  uint32_t quota_storage_mask =
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL;
  const GURL& delete_origin = GURL();
  const base::Time delete_begin = base::Time();
  base::Time delete_end = base::Time::Max();

  base::RunLoop run_loop;

  storage->ClearData(storage_partition_mask, quota_storage_mask, delete_origin,
                     StoragePartition::OriginMatcherFunction(), delete_begin,
                     delete_end, run_loop.QuitClosure());

  run_loop.Run();
}

std::string BackgroundSyncBrowserTest::PopConsoleString() {
  std::string script_result;
  EXPECT_TRUE(RunScript("resultQueue.pop()", &script_result));
  return script_result;
}

bool BackgroundSyncBrowserTest::PopConsole(const std::string& expected_msg) {
  std::string script_result = PopConsoleString();
  return script_result == expected_msg;
}

bool BackgroundSyncBrowserTest::RegisterServiceWorker() {
  std::string script_result;
  EXPECT_TRUE(RunScript("registerServiceWorker()", &script_result));
  return script_result == BuildExpectedResult("service worker", "registered");
}

bool BackgroundSyncBrowserTest::RegisterOneShot(const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(
      RunScript(BuildScriptString("registerOneShot", tag), &script_result));
  return script_result == BuildExpectedResult(tag, "registered");
}

bool BackgroundSyncBrowserTest::RegisterOneShotFromServiceWorker(
    const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(
      RunScript(BuildScriptString("registerOneShotFromServiceWorker", tag),
                &script_result));
  return script_result == BuildExpectedResult(tag, "register sent to SW");
}

bool BackgroundSyncBrowserTest::GetRegistrationOneShot(const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(RunScript(BuildScriptString("getRegistrationOneShot", tag),
                        &script_result));
  return script_result == BuildExpectedResult(tag, "found");
}

bool BackgroundSyncBrowserTest::GetRegistrationOneShotFromServiceWorker(
    const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(RunScript(
      BuildScriptString("getRegistrationOneShotFromServiceWorker", tag),
      &script_result));
  EXPECT_TRUE(script_result == "ok - getRegistration sent to SW");

  return PopConsole(BuildExpectedResult(tag, "found"));
}

bool BackgroundSyncBrowserTest::MatchRegistrations(
    const std::string& script_result,
    const std::vector<std::string>& expected_tags) {
  EXPECT_TRUE(base::StartsWith(script_result, kSuccessfulOperationPrefix,
                               base::CompareCase::INSENSITIVE_ASCII));
  std::string tag_string =
      script_result.substr(strlen(kSuccessfulOperationPrefix));
  std::vector<std::string> result_tags = base::SplitString(
      tag_string, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  return std::set<std::string>(expected_tags.begin(), expected_tags.end()) ==
         std::set<std::string>(result_tags.begin(), result_tags.end());
}

bool BackgroundSyncBrowserTest::GetRegistrationsOneShot(
    const std::vector<std::string>& expected_tags) {
  std::string script_result;
  EXPECT_TRUE(RunScript("getRegistrationsOneShot()", &script_result));

  return MatchRegistrations(script_result, expected_tags);
}

bool BackgroundSyncBrowserTest::GetRegistrationsOneShotFromServiceWorker(
    const std::vector<std::string>& expected_tags) {
  std::string script_result;
  EXPECT_TRUE(
      RunScript("getRegistrationsOneShotFromServiceWorker()", &script_result));
  EXPECT_TRUE(script_result == "ok - getRegistrations sent to SW");

  return MatchRegistrations(PopConsoleString(), expected_tags);
}

bool BackgroundSyncBrowserTest::CompleteDelayedOneShot() {
  std::string script_result;
  EXPECT_TRUE(RunScript("completeDelayedOneShot()", &script_result));
  return script_result == BuildExpectedResult("delay", "completing");
}

bool BackgroundSyncBrowserTest::RejectDelayedOneShot() {
  std::string script_result;
  EXPECT_TRUE(RunScript("rejectDelayedOneShot()", &script_result));
  return script_result == BuildExpectedResult("delay", "rejecting");
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, OneShotFiresControlled) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  EXPECT_TRUE(RegisterOneShot("foo"));
  EXPECT_TRUE(PopConsole("foo fired"));
  EXPECT_FALSE(GetRegistrationOneShot("foo"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, OneShotFiresUncontrolled) {
  EXPECT_TRUE(RegisterServiceWorker());

  EXPECT_TRUE(RegisterOneShot("foo"));
  EXPECT_TRUE(PopConsole("foo fired"));
  EXPECT_FALSE(GetRegistrationOneShot("foo"));
}

// Verify that Register works in a service worker
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       OneShotFromServiceWorkerFires) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  EXPECT_TRUE(RegisterOneShotFromServiceWorker("foo_sw"));
  EXPECT_TRUE(PopConsole("ok - foo_sw registered in SW"));
  EXPECT_TRUE(PopConsole("foo_sw fired"));
  EXPECT_FALSE(GetRegistrationOneShot("foo_sw"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, OneShotDelaysForNetwork) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  // Prevent firing by going offline.
  background_sync_test_util::SetOnline(web_contents(), false);
  EXPECT_TRUE(RegisterOneShot("foo"));
  EXPECT_TRUE(GetRegistrationOneShot("foo"));
  EXPECT_TRUE(OneShotPending("foo"));

  // Resume firing by going online.
  background_sync_test_util::SetOnline(web_contents(), true);
  EXPECT_TRUE(PopConsole("foo fired"));
  EXPECT_FALSE(GetRegistrationOneShot("foo"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, WaitUntil) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  background_sync_test_util::SetOnline(web_contents(), true);
  EXPECT_TRUE(RegisterOneShot("delay"));

  // Verify that it is firing.
  EXPECT_TRUE(GetRegistrationOneShot("delay"));
  EXPECT_FALSE(OneShotPending("delay"));

  // Complete the task.
  EXPECT_TRUE(CompleteDelayedOneShot());
  EXPECT_TRUE(PopConsole("ok - delay completed"));

  // Verify that it finished firing.
  EXPECT_FALSE(GetRegistrationOneShot("delay"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, WaitUntilReject) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  background_sync_test_util::SetOnline(web_contents(), true);
  EXPECT_TRUE(RegisterOneShot("delay"));

  // Verify that it is firing.
  EXPECT_TRUE(GetRegistrationOneShot("delay"));
  EXPECT_FALSE(OneShotPending("delay"));

  // Complete the task.
  EXPECT_TRUE(RejectDelayedOneShot());
  EXPECT_TRUE(PopConsole("ok - delay rejected"));
  EXPECT_FALSE(GetRegistrationOneShot("delay"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, Incognito) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  background_sync_test_util::SetOnline(web_contents(), false);
  EXPECT_TRUE(RegisterOneShot("normal"));
  EXPECT_TRUE(OneShotPending("normal"));

  // Go incognito and verify that incognito doesn't see the registration.
  SetIncognitoMode(true);

  // Tell the new network observer that we're offline (it initializes from
  // NetworkChangeNotifier::GetCurrentConnectionType() which is not mocked out
  // in this test).
  background_sync_test_util::SetOnline(web_contents(), false);

  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));
  EXPECT_TRUE(RegisterServiceWorker());

  EXPECT_FALSE(GetRegistrationOneShot("normal"));

  EXPECT_TRUE(RegisterOneShot("incognito"));
  EXPECT_TRUE(OneShotPending("incognito"));

  // Switch back and make sure the registration is still there.
  SetIncognitoMode(false);
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Should be controlled.

  EXPECT_TRUE(GetRegistrationOneShot("normal"));
  EXPECT_FALSE(GetRegistrationOneShot("incognito"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, GetRegistrations) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetRegistrationsOneShot(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);
  registered_tags.push_back("foo");
  registered_tags.push_back("bar");

  for (const std::string& tag : registered_tags)
    EXPECT_TRUE(RegisterOneShot(tag));

  EXPECT_TRUE(GetRegistrationsOneShot(registered_tags));
}

// Verify that GetRegistrations works in a service worker
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       GetRegistrationsFromServiceWorker) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetRegistrationsOneShot(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);
  registered_tags.push_back("foo_sw");
  registered_tags.push_back("bar_sw");

  for (const std::string& tag : registered_tags) {
    EXPECT_TRUE(RegisterOneShotFromServiceWorker(tag));
    EXPECT_TRUE(PopConsole(BuildExpectedResult(tag, "registered in SW")));
  }

  EXPECT_TRUE(GetRegistrationsOneShotFromServiceWorker(registered_tags));
}

// Verify that GetRegistration works in a service worker
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       GetRegistrationFromServiceWorker) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetRegistrationsOneShot(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);

  EXPECT_TRUE(RegisterOneShotFromServiceWorker("foo_sw"));
  EXPECT_TRUE(PopConsole("ok - foo_sw registered in SW"));
  EXPECT_TRUE(GetRegistrationOneShotFromServiceWorker("foo_sw"));
}

// Verify that a background sync registration is deleted when site data is
// cleared.
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       SyncRegistrationDeletedWhenClearingSiteData) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  // Prevent firing by going offline.
  background_sync_test_util::SetOnline(web_contents(), false);
  EXPECT_TRUE(RegisterOneShot("foo"));
  EXPECT_TRUE(GetRegistrationOneShot("foo"));
  EXPECT_TRUE(OneShotPending("foo"));

  // Simulate a user clearing site data (including Service Workers, crucially),
  // by clearing data from the storage partition.
  ClearStoragePartitionData();

  EXPECT_FALSE(GetRegistrationOneShot("foo"));
}

// Verify that a background sync registration, from a service worker, is deleted
// when site data is cleared.
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       SyncRegistrationFromSWDeletedWhenClearingSiteData) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetRegistrationsOneShot(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);

  EXPECT_TRUE(RegisterOneShotFromServiceWorker("foo_sw"));
  EXPECT_TRUE(PopConsole("ok - foo_sw registered in SW"));
  EXPECT_TRUE(GetRegistrationOneShotFromServiceWorker("foo_sw"));

  // Simulate a user clearing site data (including Service Workers, crucially),
  // by clearing data from the storage partition.
  ClearStoragePartitionData();

  EXPECT_FALSE(GetRegistrationOneShotFromServiceWorker("foo"));
}

// Verify that multiple background sync registrations are deleted when site
// data is cleared.
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       SyncRegistrationsDeletedWhenClearingSiteData) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetRegistrationsOneShot(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);
  registered_tags.push_back("foo");
  registered_tags.push_back("bar");

  for (const std::string& tag : registered_tags)
    EXPECT_TRUE(RegisterOneShot(tag));

  EXPECT_TRUE(GetRegistrationsOneShot(registered_tags));

  for (const std::string& tag : registered_tags)
    EXPECT_TRUE(OneShotPending(tag));

  // Simulate a user clearing site data (including Service Workers, crucially),
  // by clearing data from the storage partition.
  ClearStoragePartitionData();

  for (const std::string& tag : registered_tags)
    EXPECT_FALSE(GetRegistrationOneShot(tag));
}

// Verify that a sync event that is currently firing is deleted when site
// data is cleared.
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       FiringSyncEventDeletedWhenClearingSiteData) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  background_sync_test_util::SetOnline(web_contents(), true);
  EXPECT_TRUE(RegisterOneShot("delay"));

  // Verify that it is firing.
  EXPECT_TRUE(GetRegistrationOneShot("delay"));
  EXPECT_FALSE(OneShotPending("delay"));

  // Simulate a user clearing site data (including Service Workers, crucially),
  // by clearing data from the storage partition.
  ClearStoragePartitionData();

  // Verify that it was deleted.
  EXPECT_FALSE(GetRegistrationOneShot("delay"));
}

// Disabled due to flakiness. See https://crbug.com/578952.
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, DISABLED_VerifyRetry) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  SetMaxSyncAttempts(2);

  EXPECT_TRUE(RegisterOneShot("delay"));
  EXPECT_TRUE(RejectDelayedOneShot());
  EXPECT_TRUE(PopConsole("ok - delay rejected"));

  // Verify that the oneshot is still around and waiting to try again.
  EXPECT_TRUE(OneShotPending("delay"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, RegisterFromNonMainFrame) {
  std::string script_result;
  GURL url = https_server()->GetURL(kEmptyURL);
  EXPECT_TRUE(
      RunScript(BuildScriptString("registerOneShotFromLocalFrame", url.spec()),
                &script_result));
  EXPECT_EQ(BuildExpectedResult("iframe", "failed to register sync"),
            script_result);
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       RegisterFromServiceWorkerWithoutMainFrameHost) {
  // Start a second https server to use as a second origin.
  net::EmbeddedTestServer alt_server(net::EmbeddedTestServer::TYPE_HTTPS);
  alt_server.ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(alt_server.Start());

  std::string script_result;
  GURL url = alt_server.GetURL(kRegisterSyncURL);
  EXPECT_TRUE(
      RunScript(BuildScriptString("registerOneShotFromCrossOriginServiceWorker",
                                  url.spec()),
                &script_result));
  EXPECT_EQ(BuildExpectedResult("worker", "failed to register sync"),
            script_result);
}

}  // namespace content
