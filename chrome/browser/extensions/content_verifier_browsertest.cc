// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_observer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/content_verify_job.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {

namespace {

// Helper for observing extension unloads.
class UnloadObserver : public ExtensionRegistryObserver {
 public:
  explicit UnloadObserver(ExtensionRegistry* registry) : observer_(this) {
    observer_.Add(registry);
  }
  ~UnloadObserver() override {}

  void WaitForUnload(const ExtensionId& id) {
    if (ContainsKey(observed_, id))
      return;

    ASSERT_TRUE(loop_runner_.get() == NULL);
    awaited_id_ = id;
    loop_runner_ = new content::MessageLoopRunner();
    loop_runner_->Run();
  }

  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionInfo::Reason reason) override {
    observed_.insert(extension->id());
    if (awaited_id_ == extension->id())
      loop_runner_->Quit();
  }

 private:
  ExtensionId awaited_id_;
  std::set<ExtensionId> observed_;
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
  ScopedObserver<ExtensionRegistry, UnloadObserver> observer_;
};

// Helper for forcing ContentVerifyJob's to return an error.
class JobDelegate : public ContentVerifyJob::TestDelegate {
 public:
  JobDelegate() : fail_next_read_(false), fail_next_done_(false) {}

  virtual ~JobDelegate() {}

  void set_id(const ExtensionId& id) { id_ = id; }
  void fail_next_read() { fail_next_read_ = true; }
  void fail_next_done() { fail_next_done_ = true; }

  ContentVerifyJob::FailureReason BytesRead(const ExtensionId& id,
                                            int count,
                                            const char* data) override {
    if (id == id_ && fail_next_read_) {
      fail_next_read_ = false;
      return ContentVerifyJob::HASH_MISMATCH;
    }
    return ContentVerifyJob::NONE;
  }

  ContentVerifyJob::FailureReason DoneReading(const ExtensionId& id) override {
    if (id == id_ && fail_next_done_) {
      fail_next_done_ = false;
      return ContentVerifyJob::HASH_MISMATCH;
    }
    return ContentVerifyJob::NONE;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(JobDelegate);

  ExtensionId id_;
  bool fail_next_read_;
  bool fail_next_done_;
};

class JobObserver : public ContentVerifyJob::TestObserver {
 public:
  JobObserver();
  virtual ~JobObserver();

  // Call this to add an expected job result.
  void ExpectJobResult(const std::string& extension_id,
                       const base::FilePath& relative_path,
                       bool expected_to_fail);

  // Wait to see expected jobs. Returns true if we saw all jobs finish as
  // expected, or false if any job completed with non-expected success/failure
  // status.
  bool WaitForExpectedJobs();

  // ContentVerifyJob::TestObserver interface
  void JobStarted(const std::string& extension_id,
                  const base::FilePath& relative_path) override;

  void JobFinished(const std::string& extension_id,
                   const base::FilePath& relative_path,
                   bool failed) override;

 private:
  typedef std::pair<std::string, base::FilePath> ExtensionFile;
  typedef std::map<ExtensionFile, bool> ExpectedJobs;
  ExpectedJobs expected_jobs_;
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
  bool saw_expected_job_results_;
};

void JobObserver::ExpectJobResult(const std::string& extension_id,
                                  const base::FilePath& relative_path,
                                  bool expected_to_fail) {
  expected_jobs_.insert(std::make_pair(
      ExtensionFile(extension_id, relative_path), expected_to_fail));
}

JobObserver::JobObserver() : saw_expected_job_results_(false) {
}

JobObserver::~JobObserver() {
}

bool JobObserver::WaitForExpectedJobs() {
  if (!expected_jobs_.empty()) {
    loop_runner_ = new content::MessageLoopRunner();
    loop_runner_->Run();
  }
  return saw_expected_job_results_;
}

void JobObserver::JobStarted(const std::string& extension_id,
                             const base::FilePath& relative_path) {
}

void JobObserver::JobFinished(const std::string& extension_id,
                              const base::FilePath& relative_path,
                              bool failed) {
  ExpectedJobs::iterator i = expected_jobs_.find(ExtensionFile(
      extension_id, relative_path.NormalizePathSeparatorsTo('/')));
  if (i != expected_jobs_.end()) {
    if (failed != i->second) {
      saw_expected_job_results_ = false;
      if (loop_runner_.get())
        loop_runner_->Quit();
    }
    expected_jobs_.erase(i);
    if (expected_jobs_.empty()) {
      saw_expected_job_results_ = true;
      if (loop_runner_.get())
        loop_runner_->Quit();
    }
  }
}

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

  // Setup our unload observer and JobDelegate, and install a test extension.
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    ContentVerifyJob::SetDelegateForTests(NULL);
    ContentVerifyJob::SetObserverForTests(NULL);
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  virtual void OpenPageAndWaitForUnload() {
    unload_observer_.reset(
        new UnloadObserver(ExtensionRegistry::Get(profile())));
    const Extension* extension = InstallExtensionFromWebstore(
        test_data_dir_.AppendASCII("content_verifier/v1.crx"), 1);
    ASSERT_TRUE(extension);
    id_ = extension->id();
    page_url_ = extension->GetResourceURL("page.html");
    delegate_.set_id(id_);
    ContentVerifyJob::SetDelegateForTests(&delegate_);

    // This call passes false for |check_navigation_success|, because checking
    // for navigation success needs the WebContents to still exist after the
    // navigation, whereas this navigation triggers an unload which destroys
    // the WebContents.
    AddTabAtIndexToBrowser(browser(), 1, page_url_, ui::PAGE_TRANSITION_LINK,
                           false);

    unload_observer_->WaitForUnload(id_);
    ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
    int reasons = prefs->GetDisableReasons(id_);
    EXPECT_TRUE(reasons & Extension::DISABLE_CORRUPTED);

    // This needs to happen before the ExtensionRegistry gets deleted, which
    // happens before TearDownOnMainThread is called.
    unload_observer_.reset();
  }

 protected:
  JobDelegate delegate_;
  scoped_ptr<UnloadObserver> unload_observer_;
  ExtensionId id_;
  GURL page_url_;
};

IN_PROC_BROWSER_TEST_F(ContentVerifierTest, FailOnRead) {
  delegate_.fail_next_read();
  OpenPageAndWaitForUnload();
}

IN_PROC_BROWSER_TEST_F(ContentVerifierTest, FailOnDone) {
  delegate_.fail_next_done();
  OpenPageAndWaitForUnload();
}

IN_PROC_BROWSER_TEST_F(ContentVerifierTest, DotSlashPaths) {
  JobObserver job_observer;
  ContentVerifyJob::SetObserverForTests(&job_observer);
  std::string id = "hoipipabpcoomfapcecilckodldhmpgl";

  job_observer.ExpectJobResult(
      id, base::FilePath(FILE_PATH_LITERAL("background.js")), false);
  job_observer.ExpectJobResult(
      id, base::FilePath(FILE_PATH_LITERAL("page.html")), false);
  job_observer.ExpectJobResult(
      id, base::FilePath(FILE_PATH_LITERAL("page.js")), false);
  job_observer.ExpectJobResult(
      id, base::FilePath(FILE_PATH_LITERAL("dir/page2.html")), false);
  job_observer.ExpectJobResult(
      id, base::FilePath(FILE_PATH_LITERAL("page2.js")), false);

  // Install a test extension we copied from the webstore that has actual
  // signatures, and contains image paths with leading "./".
  const Extension* extension = InstallExtensionFromWebstore(
      test_data_dir_.AppendASCII("content_verifier/dot_slash_paths.crx"), 1);

  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->id(), id);

  EXPECT_TRUE(job_observer.WaitForExpectedJobs());

  ContentVerifyJob::SetObserverForTests(NULL);
}

}  // namespace extensions
