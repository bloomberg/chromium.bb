// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <set>
#include <string>

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/chrome_content_verifier_delegate.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/chrome_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/content_verify_job.h"
#include "extensions/browser/crx_file_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_downloader_test_delegate.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/common/extension_urls.h"

namespace extensions {

namespace {

// Helper for observing extension registry events.
class RegistryObserver : public ExtensionRegistryObserver {
 public:
  explicit RegistryObserver(ExtensionRegistry* registry) : observer_(this) {
    observer_.Add(registry);
  }
  ~RegistryObserver() override {}

  // Waits until we've seen an unload for extension with |id|, returning true
  // if we saw one or false otherwise (typically because of test timeout).
  bool WaitForUnload(const ExtensionId& id) {
    if (base::ContainsKey(unloaded_, id))
      return true;

    base::RunLoop run_loop;
    awaited_unload_id_ = id;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    return base::ContainsKey(unloaded_, id);
  }

  // Same as WaitForUnload, but for an install.
  bool WaitForInstall(const ExtensionId& id) {
    if (base::ContainsKey(installed_, id))
      return true;

    base::RunLoop run_loop;
    awaited_install_id_ = id;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    return base::ContainsKey(installed_, id);
  }

  // ExtensionRegistryObserver
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionInfo::Reason reason) override {
    unloaded_.insert(extension->id());
    if (awaited_unload_id_ == extension->id()) {
      awaited_unload_id_.clear();
      base::ResetAndReturn(&quit_closure_).Run();
    }
  }

  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override {
    installed_.insert(extension->id());
    if (awaited_install_id_ == extension->id()) {
      awaited_install_id_.clear();
      base::ResetAndReturn(&quit_closure_).Run();
    }
  }

 private:
  // The id we're waiting for a load/install of respectively.
  ExtensionId awaited_unload_id_;
  ExtensionId awaited_install_id_;

  // The quit closure for stopping a running RunLoop, if we're waiting.
  base::Closure quit_closure_;

  // The extension id's we've seen unloaded and installed, respectively.
  std::set<ExtensionId> unloaded_;
  std::set<ExtensionId> installed_;

  ScopedObserver<ExtensionRegistry, RegistryObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(RegistryObserver);
};

// Helper for forcing ContentVerifyJob's to return an error.
class JobDelegate : public ContentVerifyJob::TestDelegate {
 public:
  JobDelegate()
      : fail_next_read_(false),
        fail_next_done_(false),
        bytes_read_failed_(0),
        done_reading_failed_(0) {}

  virtual ~JobDelegate() {}

  void set_id(const ExtensionId& id) { id_ = id; }
  void fail_next_read() { fail_next_read_ = true; }
  void fail_next_done() { fail_next_done_ = true; }

  // Return the number of BytesRead/DoneReading calls we actually failed,
  // respectively.
  int bytes_read_failed() { return bytes_read_failed_; }
  int done_reading_failed() { return done_reading_failed_; }

  ContentVerifyJob::FailureReason BytesRead(const ExtensionId& id,
                                            int count,
                                            const char* data) override {
    if (id == id_ && fail_next_read_) {
      fail_next_read_ = false;
      bytes_read_failed_++;
      return ContentVerifyJob::HASH_MISMATCH;
    }
    return ContentVerifyJob::NONE;
  }

  ContentVerifyJob::FailureReason DoneReading(const ExtensionId& id) override {
    if (id == id_ && fail_next_done_) {
      fail_next_done_ = false;
      done_reading_failed_++;
      return ContentVerifyJob::HASH_MISMATCH;
    }
    return ContentVerifyJob::NONE;
  }

 private:
  ExtensionId id_;
  bool fail_next_read_;
  bool fail_next_done_;
  int bytes_read_failed_;
  int done_reading_failed_;

  DISALLOW_COPY_AND_ASSIGN(JobDelegate);
};

class JobObserver : public ContentVerifyJob::TestObserver {
 public:
  JobObserver();
  virtual ~JobObserver();

  enum class Result { SUCCESS, FAILURE };

  // Call this to add an expected job result.
  void ExpectJobResult(const std::string& extension_id,
                       const base::FilePath& relative_path,
                       Result expected_result);

  // Wait to see expected jobs. Returns true when we've seen all expected jobs
  // finish, or false if there was an error or timeout.
  bool WaitForExpectedJobs();

  // ContentVerifyJob::TestObserver interface
  void JobStarted(const std::string& extension_id,
                  const base::FilePath& relative_path) override;

  void JobFinished(const std::string& extension_id,
                   const base::FilePath& relative_path,
                   bool failed) override;

 private:
  struct ExpectedResult {
   public:
    std::string extension_id;
    base::FilePath path;
    Result result;

    ExpectedResult(const std::string& extension_id, const base::FilePath& path,
                   Result result) {
      this->extension_id = extension_id;
      this->path = path;
      this->result = result;
    }
  };
  std::list<ExpectedResult> expectations_;
  content::BrowserThread::ID creation_thread_;
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
};

void JobObserver::ExpectJobResult(const std::string& extension_id,
                                  const base::FilePath& relative_path,
                                  Result expected_result) {
  expectations_.push_back(ExpectedResult(
      extension_id, relative_path, expected_result));
}

JobObserver::JobObserver() {
  EXPECT_TRUE(
      content::BrowserThread::GetCurrentThreadIdentifier(&creation_thread_));
}

JobObserver::~JobObserver() {
}

bool JobObserver::WaitForExpectedJobs() {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(creation_thread_));
  if (!expectations_.empty()) {
    loop_runner_ = new content::MessageLoopRunner();
    loop_runner_->Run();
    loop_runner_ = nullptr;
  }
  return expectations_.empty();
}

void JobObserver::JobStarted(const std::string& extension_id,
                             const base::FilePath& relative_path) {
}

void JobObserver::JobFinished(const std::string& extension_id,
                              const base::FilePath& relative_path,
                              bool failed) {
  if (!content::BrowserThread::CurrentlyOn(creation_thread_)) {
    content::BrowserThread::PostTask(
        creation_thread_, FROM_HERE,
        base::Bind(&JobObserver::JobFinished, base::Unretained(this),
                   extension_id, relative_path, failed));
    return;
  }
  Result result = failed ? Result::FAILURE : Result::SUCCESS;
  bool found = false;
  for (std::list<ExpectedResult>::iterator i = expectations_.begin();
       i != expectations_.end(); ++i) {
    if (i->extension_id == extension_id && i->path == relative_path &&
        i->result == result) {
      found = true;
      expectations_.erase(i);
      break;
    }
  }
  if (found) {
    if (expectations_.empty() && loop_runner_.get())
      loop_runner_->Quit();
  } else {
    LOG(WARNING) << "Ignoring unexpected JobFinished " << extension_id << "/"
                 << relative_path.value() << " failed:" << failed;
  }
}

class VerifierObserver : public ContentVerifier::TestObserver {
 public:
  VerifierObserver();
  virtual ~VerifierObserver();

  const std::set<std::string>& completed_fetches() {
    return completed_fetches_;
  }

  // Returns when we've seen OnFetchComplete for |extension_id|.
  void WaitForFetchComplete(const std::string& extension_id);

  // ContentVerifier::TestObserver
  void OnFetchComplete(const std::string& extension_id, bool success) override;

 private:
  std::set<std::string> completed_fetches_;
  std::string id_to_wait_for_;
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
};

VerifierObserver::VerifierObserver() {
}

VerifierObserver::~VerifierObserver() {
}

void VerifierObserver::WaitForFetchComplete(const std::string& extension_id) {
  EXPECT_TRUE(id_to_wait_for_.empty());
  EXPECT_EQ(loop_runner_.get(), nullptr);
  id_to_wait_for_ = extension_id;
  loop_runner_ = new content::MessageLoopRunner();
  loop_runner_->Run();
  id_to_wait_for_.clear();
  loop_runner_ = nullptr;
}

void VerifierObserver::OnFetchComplete(const std::string& extension_id,
                                       bool success) {
  completed_fetches_.insert(extension_id);
  if (extension_id == id_to_wait_for_)
    loop_runner_->Quit();
}

// This lets us intercept requests for update checks of extensions, and
// substitute a local file as a simulated response.
class DownloaderTestDelegate : public ExtensionDownloaderTestDelegate {
 public:
  DownloaderTestDelegate() {}

  // This makes it so that update check requests for |extension_id| will return
  // a downloaded file of |crx_path| that is claimed to have version
  // |version_string|.
  void AddResponse(const ExtensionId& extension_id,
                   const std::string& version_string,
                   const base::FilePath& crx_path) {
    responses_[extension_id] = std::make_pair(version_string, crx_path);
  }

  const std::vector<std::unique_ptr<ManifestFetchData>>& requests() {
    return requests_;
  }

  // ExtensionDownloaderTestDelegate:
  void StartUpdateCheck(
      ExtensionDownloader* downloader,
      ExtensionDownloaderDelegate* delegate,
      std::unique_ptr<ManifestFetchData> fetch_data) override {
    requests_.push_back(std::move(fetch_data));
    const ManifestFetchData* data = requests_.back().get();

    for (const auto& id : data->extension_ids()) {
      if (ContainsKey(responses_, id)) {
        // We use PostTask here instead of calling OnExtensionDownloadFinished
        // immeditately, because the calling code isn't expecting a synchronous
        // response (in non-test situations there are at least 2 network
        // requests needed before a file could be returned).
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::Bind(
                &ExtensionDownloaderDelegate::OnExtensionDownloadFinished,
                base::Unretained(delegate),
                CRXFileInfo(id, responses_[id].second),
                false /* pass_file_ownership */, GURL(), responses_[id].first,
                ExtensionDownloaderDelegate::PingResult(), data->request_ids(),
                ExtensionDownloaderDelegate::InstallCallback()));
      }
    }
  }

 private:
  // The requests we've received.
  std::vector<std::unique_ptr<ManifestFetchData>> requests_;

  // The prepared responses - this maps an extension id to a (version string,
  // crx file path) pair.
  std::map<std::string, std::pair<ExtensionId, base::FilePath>> responses_;

  DISALLOW_COPY_AND_ASSIGN(DownloaderTestDelegate);
};

// This lets us simulate the behavior of an enterprise policy that wants
// a given extension to be installed via the webstore.
class TestExternalProvider : public ExternalProviderInterface {
 public:
  TestExternalProvider(VisitorInterface* visitor,
                       const ExtensionId& extension_id)
      : visitor_(visitor), extension_id_(extension_id) {}

  ~TestExternalProvider() override {}

  // ExternalProviderInterface:
  void ServiceShutdown() override {}

  void VisitRegisteredExtension() override {
    visitor_->OnExternalExtensionUpdateUrlFound(
        ExternalInstallInfoUpdateUrl(
            extension_id_, std::string() /* install_parameter */,
            base::MakeUnique<GURL>(extension_urls::GetWebstoreUpdateUrl()),
            Manifest::EXTERNAL_POLICY_DOWNLOAD, 0 /* creation_flags */,
            true /* mark_acknowledged */),
        true /* is_initial_load */);
    visitor_->OnExternalProviderReady(this);
  }

  bool HasExtension(const ExtensionId& id) const override {
    return id == std::string("npnbmohejbjohgpjnmjagbafnjhkmgko");
  }

  bool GetExtensionDetails(
      const ExtensionId& id,
      Manifest::Location* location,
      std::unique_ptr<base::Version>* version) const override {
    ADD_FAILURE() << "Unexpected GetExtensionDetails call; id:" << id;
    return false;
  }

  bool IsReady() const override { return true; }

 private:
  VisitorInterface* visitor_;
  ExtensionId extension_id_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(TestExternalProvider);
};

// This lets us simulate a policy-installed extension being "force" installed;
// ie a user is not allowed to manually uninstall/disable it.
class ForceInstallProvider : public ManagementPolicy::Provider {
 public:
  explicit ForceInstallProvider(const ExtensionId& id) : id_(id) {}
  ~ForceInstallProvider() override {}

  std::string GetDebugPolicyProviderName() const override {
    return "ForceInstallProvider";
  }

  // MananagementPolicy::Provider:
  bool UserMayModifySettings(const Extension* extension,
                             base::string16* error) const override {
    return extension->id() != id_;
  }
  bool MustRemainEnabled(const Extension* extension,
                         base::string16* error) const override {
    return extension->id() == id_;
  }

 private:
  // The extension id we want to disallow uninstall/disable for.
  ExtensionId id_;

  DISALLOW_COPY_AND_ASSIGN(ForceInstallProvider);
};

}  // namespace

class ContentVerifierTest : public ExtensionBrowserTest {
 public:
  ContentVerifierTest() {}
  ~ContentVerifierTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kExtensionContentVerification,
        switches::kExtensionContentVerificationEnforce);
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    ContentVerifier::SetObserverForTests(NULL);
    ContentVerifyJob::SetDelegateForTests(NULL);
    ContentVerifyJob::SetObserverForTests(NULL);
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  virtual void OpenPageAndWaitForUnload() {
    ContentVerifyJob::SetDelegateForTests(&delegate_);
    std::string id = "npnbmohejbjohgpjnmjagbafnjhkmgko";
    delegate_.set_id(id);
    unload_observer_.reset(
        new RegistryObserver(ExtensionRegistry::Get(profile())));
    const Extension* extension = InstallExtensionFromWebstore(
        test_data_dir_.AppendASCII("content_verifier/v1.crx"), 1);
    ASSERT_TRUE(extension);
    ASSERT_EQ(id, extension->id());
    page_url_ = extension->GetResourceURL("page.html");

    // This call passes false for |check_navigation_success|, because checking
    // for navigation success needs the WebContents to still exist after the
    // navigation, whereas this navigation triggers an unload which destroys
    // the WebContents.
    AddTabAtIndexToBrowser(browser(), 1, page_url_, ui::PAGE_TRANSITION_LINK,
                           false);

    EXPECT_TRUE(unload_observer_->WaitForUnload(id));
    ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
    int reasons = prefs->GetDisableReasons(id);
    EXPECT_TRUE(reasons & Extension::DISABLE_CORRUPTED);

    // This needs to happen before the ExtensionRegistry gets deleted, which
    // happens before TearDownOnMainThread is called.
    unload_observer_.reset();
  }

 protected:
  JobDelegate delegate_;
  std::unique_ptr<RegistryObserver> unload_observer_;
  GURL page_url_;
};

IN_PROC_BROWSER_TEST_F(ContentVerifierTest, FailOnRead) {
  EXPECT_EQ(0, delegate_.bytes_read_failed());
  delegate_.fail_next_read();
  OpenPageAndWaitForUnload();
  EXPECT_EQ(1, delegate_.bytes_read_failed());
}

IN_PROC_BROWSER_TEST_F(ContentVerifierTest, FailOnDone) {
  EXPECT_EQ(0, delegate_.done_reading_failed());
  delegate_.fail_next_done();
  OpenPageAndWaitForUnload();
  EXPECT_EQ(1, delegate_.done_reading_failed());
}

IN_PROC_BROWSER_TEST_F(ContentVerifierTest, DotSlashPaths) {
  JobObserver job_observer;
  ContentVerifyJob::SetObserverForTests(&job_observer);
  std::string id = "hoipipabpcoomfapcecilckodldhmpgl";

  job_observer.ExpectJobResult(
      id, base::FilePath(FILE_PATH_LITERAL("background.js")),
      JobObserver::Result::SUCCESS);
  job_observer.ExpectJobResult(id,
                               base::FilePath(FILE_PATH_LITERAL("page.html")),
                               JobObserver::Result::SUCCESS);
  job_observer.ExpectJobResult(id, base::FilePath(FILE_PATH_LITERAL("page.js")),
                               JobObserver::Result::SUCCESS);
  job_observer.ExpectJobResult(
      id, base::FilePath(FILE_PATH_LITERAL("dir/page2.html")),
      JobObserver::Result::SUCCESS);
  job_observer.ExpectJobResult(id,
                               base::FilePath(FILE_PATH_LITERAL("page2.js")),
                               JobObserver::Result::SUCCESS);
  job_observer.ExpectJobResult(id, base::FilePath(FILE_PATH_LITERAL("cs1.js")),
                               JobObserver::Result::SUCCESS);
  job_observer.ExpectJobResult(id, base::FilePath(FILE_PATH_LITERAL("cs2.js")),
                               JobObserver::Result::SUCCESS);

  VerifierObserver verifier_observer;
  ContentVerifier::SetObserverForTests(&verifier_observer);

  // Install a test extension we copied from the webstore that has actual
  // signatures, and contains paths with a leading "./" in various places.
  const Extension* extension = InstallExtensionFromWebstore(
      test_data_dir_.AppendASCII("content_verifier/dot_slash_paths.crx"), 1);

  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->id(), id);

  // The content scripts might fail verification the first time since the
  // one-time processing might not be finished yet - if that's the case then
  // we want to wait until that work is done.
  if (!base::ContainsKey(verifier_observer.completed_fetches(), id))
    verifier_observer.WaitForFetchComplete(id);

  // Now disable/re-enable the extension to cause the content scripts to be
  // read again.
  DisableExtension(id);
  EnableExtension(id);

  EXPECT_TRUE(job_observer.WaitForExpectedJobs());

  ContentVerifyJob::SetObserverForTests(NULL);
}

IN_PROC_BROWSER_TEST_F(ContentVerifierTest, ContentScripts) {
  VerifierObserver verifier_observer;
  ContentVerifier::SetObserverForTests(&verifier_observer);

  // Install an extension with content scripts. The initial read of the content
  // scripts will fail verification because they are read before the content
  // verification system has completed a one-time processing of the expected
  // hashes. (The extension only contains the root level hashes of the merkle
  // tree, but the content verification system builds the entire tree and
  // caches it in the extension install directory - see ContentHashFetcher for
  // more details).
  std::string id = "jmllhlobpjcnnomjlipadejplhmheiif";
  const Extension* extension = InstallExtensionFromWebstore(
      test_data_dir_.AppendASCII("content_verifier/content_script.crx"), 1);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->id(), id);

  // Wait for the content verification code to finish processing the hashes.
  if (!base::ContainsKey(verifier_observer.completed_fetches(), id))
    verifier_observer.WaitForFetchComplete(id);

  // Now disable the extension, since content scripts are read at enable time,
  // set up our job observer, and re-enable, expecting a success this time.
  DisableExtension(id);
  JobObserver job_observer;
  ContentVerifyJob::SetObserverForTests(&job_observer);
  job_observer.ExpectJobResult(id,
                               base::FilePath(FILE_PATH_LITERAL("script.js")),
                               JobObserver::Result::SUCCESS);
  EnableExtension(id);
  EXPECT_TRUE(job_observer.WaitForExpectedJobs());

  // Now alter the contents of the content script, reload the extension, and
  // expect to see a job failure due to the content script content hash not
  // being what was signed by the webstore.
  base::FilePath scriptfile = extension->path().AppendASCII("script.js");
  std::string extra = "some_extra_function_call();";
  ASSERT_TRUE(base::AppendToFile(scriptfile, extra.data(), extra.size()));
  DisableExtension(id);
  job_observer.ExpectJobResult(id,
                               base::FilePath(FILE_PATH_LITERAL("script.js")),
                               JobObserver::Result::FAILURE);
  EnableExtension(id);
  EXPECT_TRUE(job_observer.WaitForExpectedJobs());

  ContentVerifyJob::SetObserverForTests(NULL);
}

// Tests the case of a corrupt extension that is force-installed by policy and
// should not be allowed to be manually uninstalled/disabled by the user.
IN_PROC_BROWSER_TEST_F(ContentVerifierTest, PolicyCorrupted) {
  ExtensionSystem* system = ExtensionSystem::Get(profile());
  ExtensionService* service = system->extension_service();

  // The id of our test extension.
  std::string id("npnbmohejbjohgpjnmjagbafnjhkmgko");

  // Setup fake policy and update check objects.
  ForceInstallProvider policy(id);
  DownloaderTestDelegate downloader;
  system->management_policy()->RegisterProvider(&policy);
  ExtensionDownloader::set_test_delegate(&downloader);
  service->AddProviderForTesting(
      base::MakeUnique<TestExternalProvider>(service, id));

  base::FilePath crx_path =
      test_data_dir_.AppendASCII("content_verifier/v1.crx");
  const Extension* extension =
      InstallExtension(crx_path, 1, Manifest::EXTERNAL_POLICY_DOWNLOAD);
  EXPECT_NE(extension, nullptr);

  downloader.AddResponse(id, extension->VersionString(), crx_path);

  RegistryObserver registry_observer(ExtensionRegistry::Get(profile()));
  ContentVerifier* verifier = system->content_verifier();
  verifier->VerifyFailed(extension->id(), ContentVerifyJob::HASH_MISMATCH);

  // Make sure the extension first got disabled due to corruption.
  EXPECT_TRUE(registry_observer.WaitForUnload(id));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  int reasons = prefs->GetDisableReasons(id);
  EXPECT_TRUE(reasons & Extension::DISABLE_CORRUPTED);

  // Make sure the extension then got re-installed, and that after reinstall it
  // is no longer disabled due to corruption.
  EXPECT_TRUE(registry_observer.WaitForInstall(id));
  reasons = prefs->GetDisableReasons(id);
  EXPECT_FALSE(reasons & Extension::DISABLE_CORRUPTED);

  // Make sure that the update check request properly included a parameter
  // indicating that this was a corrupt policy reinstall.
  bool found = false;
  for (const auto& request : downloader.requests()) {
    if (request->Includes(id)) {
      std::string query = request->full_url().query();
      for (const auto& part : base::SplitString(
               query, "&", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
        if (base::StartsWith(part, "x=", base::CompareCase::SENSITIVE) &&
            part.find(std::string("id%3D") + id) != std::string::npos) {
          found = true;
          EXPECT_NE(std::string::npos, part.find("installsource%3Dreinstall"));
        }
      }
    }
  }
  EXPECT_TRUE(found);
}

class ContentVerifierPolicyTest : public ContentVerifierTest {
 public:
  // We need to do this work here because the force-install policy values are
  // checked pretty early on in the startup of the ExtensionService, which
  // happens between SetUpInProcessBrowserTestFixture and SetUpOnMainThread.
  void SetUpInProcessBrowserTestFixture() override {
    ContentVerifierTest::SetUpInProcessBrowserTestFixture();

    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    ExtensionManagementPolicyUpdater management_policy(&policy_provider_);
    management_policy.SetIndividualExtensionAutoInstalled(
        id_, extension_urls::kChromeWebstoreUpdateURL, true /* forced */);

    ExtensionDownloader::set_test_delegate(&downloader_);
    base::FilePath crx_path =
        test_data_dir_.AppendASCII("content_verifier/v1.crx");
    std::string version = "2";
    downloader_.AddResponse(id_, version, crx_path);
  }

  void SetUpOnMainThread() override {
    extensions::browsertest_util::CreateAndInitializeLocalCache();
  }

 protected:
  // The id of the extension we want to have force-installed.
  std::string id_ = "npnbmohejbjohgpjnmjagbafnjhkmgko";

 private:
  policy::MockConfigurationPolicyProvider policy_provider_;
  DownloaderTestDelegate downloader_;
};

// We want to test what happens at startup with a corroption-disabled policy
// force installed extension. So we set that up in the PRE test here.
IN_PROC_BROWSER_TEST_F(ContentVerifierPolicyTest,
                       PRE_PolicyCorruptedOnStartup) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  RegistryObserver registry_observer(registry);

  // Wait for the extension to be installed by policy we set up in
  // SetUpInProcessBrowserTestFixture.
  if (!registry->GetInstalledExtension(id_)) {
    EXPECT_TRUE(registry_observer.WaitForInstall(id_));
  }

  // Simulate corruption of the extension so that we can test what happens
  // at startup in the non-PRE test.
  ExtensionSystem* system = ExtensionSystem::Get(profile());
  ContentVerifier* verifier = system->content_verifier();
  verifier->VerifyFailed(id_, ContentVerifyJob::HASH_MISMATCH);
  EXPECT_TRUE(registry_observer.WaitForUnload(id_));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  int reasons = prefs->GetDisableReasons(id_);
  EXPECT_TRUE(reasons & Extension::DISABLE_CORRUPTED);
}

// Now actually test what happens on the next startup after the PRE test above.
IN_PROC_BROWSER_TEST_F(ContentVerifierPolicyTest, PolicyCorruptedOnStartup) {
  // Depdending on timing, the extension may have already been reinstalled
  // between SetUpInProcessBrowserTestFixture and now (usually not during local
  // testing on a developer machine, but sometimes on a heavily loaded system
  // such as the build waterfall / trybots). If the reinstall didn't already
  // happen, wait for it.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  int disable_reasons = prefs->GetDisableReasons(id_);
  if (disable_reasons & Extension::DISABLE_CORRUPTED) {
    RegistryObserver registry_observer(registry);
    EXPECT_TRUE(registry_observer.WaitForInstall(id_));
    disable_reasons = prefs->GetDisableReasons(id_);
  }
  EXPECT_FALSE(disable_reasons & Extension::DISABLE_CORRUPTED);
  EXPECT_TRUE(registry->enabled_extensions().Contains(id_));
}

namespace {

// A helper for intercepting the normal action that
// ChromeContentVerifierDelegate would take on discovering corruption, letting
// us track the delay for each consecutive reinstall.
class DelayTracker {
 public:
  DelayTracker() {}

  const std::vector<base::TimeDelta>& calls() { return calls_; }

  void ReinstallAction(base::TimeDelta delay) { calls_.push_back(delay); }

 private:
  std::vector<base::TimeDelta> calls_;

  DISALLOW_COPY_AND_ASSIGN(DelayTracker);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ContentVerifierPolicyTest, Backoff) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ExtensionSystem* system = ExtensionSystem::Get(profile());
  ExtensionService* service = system->extension_service();
  ContentVerifier* verifier = system->content_verifier();

  // Wait for the extension to be installed by the policy we set up in
  // SetUpInProcessBrowserTestFixture.
  if (!registry->GetInstalledExtension(id_)) {
    RegistryObserver registry_observer(registry);
    EXPECT_TRUE(registry_observer.WaitForInstall(id_));
  }

  // Setup to intercept reinstall action, so we can see what the delay would
  // have been for the real action.
  DelayTracker delay_tracker;
  base::Callback<void(base::TimeDelta)> action = base::Bind(
      &DelayTracker::ReinstallAction, base::Unretained(&delay_tracker));
  ChromeContentVerifierDelegate::set_policy_reinstall_action_for_test(&action);

  // Do 4 iterations of disabling followed by reinstall.
  const size_t iterations = 4;
  for (size_t i = 0; i < iterations; i++) {
    RegistryObserver registry_observer(registry);
    verifier->VerifyFailed(id_, ContentVerifyJob::HASH_MISMATCH);
    EXPECT_TRUE(registry_observer.WaitForUnload(id_));
    // Trigger reinstall manually (since we overrode default reinstall action).
    service->CheckForExternalUpdates();
    EXPECT_TRUE(registry_observer.WaitForInstall(id_));
  }
  const std::vector<base::TimeDelta>& calls = delay_tracker.calls();

  // Assert that the first reinstall action happened with a delay of 0, and
  // then kept growing each additional time.
  ASSERT_EQ(iterations, calls.size());
  EXPECT_EQ(base::TimeDelta(), delay_tracker.calls()[0]);
  for (size_t i = 1; i < delay_tracker.calls().size(); i++) {
    EXPECT_LT(calls[i - 1], calls[i]);
  }
}

}  // namespace extensions
