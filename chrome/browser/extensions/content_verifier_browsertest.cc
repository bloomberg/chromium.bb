// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_observer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/content_verifier.h"
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

  enum class Result { SUCCESS, FAILURE };

  // Call this to add an expected job result.
  void ExpectJobResult(const std::string& extension_id,
                       const base::FilePath& relative_path,
                       Result expected_result);

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
  typedef std::map<ExtensionFile, Result> ExpectedJobs;
  ExpectedJobs expected_jobs_;
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
  // Used to track when jobs unexpectedly fail when expected to succeed, or
  // vice versa.
  int unexpected_job_results_;
  content::BrowserThread::ID creation_thread_;
};

void JobObserver::ExpectJobResult(const std::string& extension_id,
                                  const base::FilePath& relative_path,
                                  Result expected_result) {
  expected_jobs_.insert(std::make_pair(
      ExtensionFile(extension_id, relative_path), expected_result));
}

JobObserver::JobObserver() : unexpected_job_results_(0) {
  EXPECT_TRUE(
      content::BrowserThread::GetCurrentThreadIdentifier(&creation_thread_));
}

JobObserver::~JobObserver() {
}

bool JobObserver::WaitForExpectedJobs() {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(creation_thread_));
  if (!expected_jobs_.empty()) {
    loop_runner_ = new content::MessageLoopRunner();
    loop_runner_->Run();
    loop_runner_ = nullptr;
  }
  return unexpected_job_results_ == 0;
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
  ExpectedJobs::iterator i = expected_jobs_.find(ExtensionFile(
      extension_id, relative_path.NormalizePathSeparatorsTo('/')));
  if (i != expected_jobs_.end()) {
    if ((failed && i->second != Result::FAILURE) ||
        (!failed && i->second != Result::SUCCESS)) {
      unexpected_job_results_ += 1;
      if (loop_runner_.get())
        loop_runner_->Quit();
    }
    expected_jobs_.erase(i);
    if (expected_jobs_.empty()) {
      if (loop_runner_.get())
        loop_runner_->Quit();
    }
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

  // Install a test extension we copied from the webstore that has actual
  // signatures, and contains image paths with leading "./".
  const Extension* extension = InstallExtensionFromWebstore(
      test_data_dir_.AppendASCII("content_verifier/dot_slash_paths.crx"), 1);

  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->id(), id);

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
  if (!ContainsKey(verifier_observer.completed_fetches(), id))
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

}  // namespace extensions
