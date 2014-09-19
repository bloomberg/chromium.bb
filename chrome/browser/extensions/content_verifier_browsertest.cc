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
  virtual ~UnloadObserver() {}

  void WaitForUnload(const ExtensionId& id) {
    if (ContainsKey(observed_, id))
      return;

    ASSERT_TRUE(loop_runner_.get() == NULL);
    awaited_id_ = id;
    loop_runner_ = new content::MessageLoopRunner();
    loop_runner_->Run();
  }

  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE {
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

  virtual ContentVerifyJob::FailureReason BytesRead(const ExtensionId& id,
                                                    int count,
                                                    const char* data) OVERRIDE {
    if (id == id_ && fail_next_read_) {
      fail_next_read_ = false;
      return ContentVerifyJob::HASH_MISMATCH;
    }
    return ContentVerifyJob::NONE;
  }

  virtual ContentVerifyJob::FailureReason DoneReading(
      const ExtensionId& id) OVERRIDE {
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

}  // namespace

class ContentVerifierTest : public ExtensionBrowserTest {
 public:
  ContentVerifierTest() {}
  virtual ~ContentVerifierTest() {}

  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kExtensionContentVerification,
        switches::kExtensionContentVerificationEnforce);
  }

  // Setup our unload observer and JobDelegate, and install a test extension.
  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionBrowserTest::SetUpOnMainThread();
    unload_observer_.reset(
        new UnloadObserver(ExtensionRegistry::Get(profile())));
    const Extension* extension = InstallExtensionFromWebstore(
        test_data_dir_.AppendASCII("content_verifier/v1.crx"), 1);
    ASSERT_TRUE(extension);
    id_ = extension->id();
    page_url_ = extension->GetResourceURL("page.html");
    delegate_.set_id(id_);
    ContentVerifyJob::SetDelegateForTests(&delegate_);
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    ContentVerifyJob::SetDelegateForTests(NULL);
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  virtual void OpenPageAndWaitForUnload() {
    AddTabAtIndex(1, page_url_, ui::PAGE_TRANSITION_LINK);
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

}  // namespace extensions
