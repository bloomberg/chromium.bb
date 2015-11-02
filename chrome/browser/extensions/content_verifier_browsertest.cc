// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <set>
#include <string>

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
        new UnloadObserver(ExtensionRegistry::Get(profile())));
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

    unload_observer_->WaitForUnload(id);
    ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
    int reasons = prefs->GetDisableReasons(id);
    EXPECT_TRUE(reasons & Extension::DISABLE_CORRUPTED);

    // This needs to happen before the ExtensionRegistry gets deleted, which
    // happens before TearDownOnMainThread is called.
    unload_observer_.reset();
  }

 protected:
  JobDelegate delegate_;
  scoped_ptr<UnloadObserver> unload_observer_;
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
  if (!ContainsKey(verifier_observer.completed_fetches(), id))
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
