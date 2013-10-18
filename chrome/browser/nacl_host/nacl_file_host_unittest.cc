// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_file_host.h"

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/nacl_host/nacl_browser.h"
#include "components/nacl/common/nacl_browser_delegate.h"
#include "components/nacl/common/pnacl_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using nacl_file_host::PnaclCanOpenFile;
using nacl_file_host::EnsurePnaclInstalled;

class TestNaClBrowserDelegate : public NaClBrowserDelegate {
 public:

  TestNaClBrowserDelegate() : should_pnacl_install_succeed_(false) { }

  virtual void ShowNaClInfobar(int render_process_id,
                               int render_view_id,
                               int error_id) OVERRIDE {
  }

  virtual bool DialogsAreSuppressed() OVERRIDE {
    return false;
  }

  virtual bool GetCacheDirectory(base::FilePath* cache_dir) OVERRIDE {
    return false;
  }

  virtual bool GetPluginDirectory(base::FilePath* plugin_dir) OVERRIDE {
    return false;
  }

  virtual bool GetPnaclDirectory(base::FilePath* pnacl_dir) OVERRIDE {
    *pnacl_dir = pnacl_path_;
    return true;
  }

  virtual bool GetUserDirectory(base::FilePath* user_dir) OVERRIDE {
    return false;
  }

  virtual std::string GetVersionString() const OVERRIDE {
    return std::string();
  }

  virtual ppapi::host::HostFactory* CreatePpapiHostFactory(
      content::BrowserPpapiHost* ppapi_host) OVERRIDE {
    return NULL;
  }

  virtual bool MapUrlToLocalFilePath(const GURL& file_url,
                                     bool use_blocking_api,
                                     base::FilePath* file_path) OVERRIDE {
    return false;
  }

  virtual void TryInstallPnacl(
      const base::Callback<void(bool)>& installed) OVERRIDE {
    installed.Run(should_pnacl_install_succeed_);
  }

  void SetPnaclDirectory(const base::FilePath& pnacl_dir) {
    pnacl_path_ = pnacl_dir;
  }

  // Indicate if we should mock the PNaCl install as succeeding
  // or failing for the next test.
  void SetShouldPnaclInstallSucceed(bool succeed) {
    should_pnacl_install_succeed_ = succeed;
  }

 private:
  base::FilePath pnacl_path_;
  bool should_pnacl_install_succeed_;
};

class NaClFileHostTest : public testing::Test {
 protected:
  NaClFileHostTest();
  virtual ~NaClFileHostTest();

  virtual void SetUp() OVERRIDE {
    nacl_browser_delegate_ = new TestNaClBrowserDelegate;
    NaClBrowser::SetDelegate(nacl_browser_delegate_);
  }

  virtual void TearDown() OVERRIDE {
    NaClBrowser::SetDelegate(NULL);  // This deletes nacl_browser_delegate_
  }

  TestNaClBrowserDelegate* nacl_browser_delegate() {
    return nacl_browser_delegate_;
  }

  bool install_success() { return install_success_; }
  size_t install_call_count() {
    return std::count(events_.begin(), events_.end(), INSTALL_DONE);
  }
  size_t progress_call_count() {
    return std::count(events_.begin(), events_.end(), INSTALL_PROGRESS);
  }
  bool events_in_correct_order() {
    // INSTALL_DONE should be the last thing.
    // The rest should be progress events.
    size_t size = events_.size();
    return size > 0 && events_[size - 1] == INSTALL_DONE
        && progress_call_count() == (size - 1);
  }

 public:  // Allow classes to bind these callback methods.
  void CallbackInstall(bool success) {
    install_success_ = success;
    events_.push_back(INSTALL_DONE);
  }

  void CallbackProgress(const nacl::PnaclInstallProgress& p) {
    // Check that the first event has an unknown total.
    if (events_.size() == 0) {
      EXPECT_FALSE(nacl::PnaclInstallProgress::progress_known(p));
    }
    events_.push_back(INSTALL_PROGRESS);
    // TODO(jvoung): be able to check that current_progress
    // goes up monotonically and hits total_progress at the end,
    // when we actually get real progress events.
  }

 private:
  enum EventType {
    INSTALL_DONE,
    INSTALL_PROGRESS
  };
  TestNaClBrowserDelegate* nacl_browser_delegate_;
  bool install_success_;
  std::vector<EventType> events_;
  content::TestBrowserThreadBundle thread_bundle_;
  DISALLOW_COPY_AND_ASSIGN(NaClFileHostTest);
};

NaClFileHostTest::NaClFileHostTest()
    : nacl_browser_delegate_(NULL),
      install_success_(false),
      thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
}

NaClFileHostTest::~NaClFileHostTest() {
}

// Try to pass a few funny filenames with a dummy PNaCl directory set.
TEST_F(NaClFileHostTest, TestFilenamesWithPnaclPath) {
  base::ScopedTempDir scoped_tmp_dir;
  ASSERT_TRUE(scoped_tmp_dir.CreateUniqueTempDir());

  base::FilePath kTestPnaclPath = scoped_tmp_dir.path();

  nacl_browser_delegate()->SetPnaclDirectory(kTestPnaclPath);
  ASSERT_TRUE(NaClBrowser::GetDelegate()->GetPnaclDirectory(&kTestPnaclPath));

  // Check allowed strings, and check that the expected prefix is added.
  base::FilePath out_path;
  EXPECT_TRUE(PnaclCanOpenFile("pnacl_json", &out_path));
  base::FilePath expected_path = kTestPnaclPath.Append(
      FILE_PATH_LITERAL("pnacl_public_pnacl_json"));
  EXPECT_EQ(expected_path, out_path);

  EXPECT_TRUE(PnaclCanOpenFile("x86_32_llc", &out_path));
  expected_path = kTestPnaclPath.Append(
      FILE_PATH_LITERAL("pnacl_public_x86_32_llc"));
  EXPECT_EQ(expected_path, out_path);

  // Check character ranges.
  EXPECT_FALSE(PnaclCanOpenFile(".xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("/xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("x/chars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("\\xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("x\\chars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("$xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("%xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("CAPS", &out_path));
  const char non_ascii[] = "\xff\xfe";
  EXPECT_FALSE(PnaclCanOpenFile(non_ascii, &out_path));

  // Check file length restriction.
  EXPECT_FALSE(PnaclCanOpenFile("thisstringisactuallywaaaaaaaaaaaaaaaaaaaaaaaa"
                                "toolongwaytoolongwaaaaayyyyytoooooooooooooooo"
                                "looooooooong",
                                &out_path));

  // Other bad files.
  EXPECT_FALSE(PnaclCanOpenFile(std::string(), &out_path));
  EXPECT_FALSE(PnaclCanOpenFile(".", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("..", &out_path));
#if defined(OS_WIN)
  EXPECT_FALSE(PnaclCanOpenFile("..\\llc", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("%SystemRoot%", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("%SystemRoot%\\explorer.exe", &out_path));
#else
  EXPECT_FALSE(PnaclCanOpenFile("../llc", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("/bin/sh", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("$HOME", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("$HOME/.bashrc", &out_path));
#endif
}

// Test that callbacks return success when PNaCl looks like it is
// already installed. No intermediate progress events are expected.
TEST_F(NaClFileHostTest, TestEnsureInstalledAlreadyInstalled) {
  base::ScopedTempDir scoped_tmp_dir;
  ASSERT_TRUE(scoped_tmp_dir.CreateUniqueTempDir());

  base::FilePath kTestPnaclPath = scoped_tmp_dir.path();
  nacl_browser_delegate()->SetPnaclDirectory(kTestPnaclPath);
  ASSERT_TRUE(NaClBrowser::GetDelegate()->GetPnaclDirectory(&kTestPnaclPath));

  EnsurePnaclInstalled(
      base::Bind(&NaClFileHostTest::CallbackInstall, base::Unretained(this)),
      base::Bind(&NaClFileHostTest::CallbackProgress, base::Unretained(this)));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(install_success());
  EXPECT_EQ(1u, install_call_count());
  EXPECT_EQ(0u, progress_call_count());
  EXPECT_TRUE(events_in_correct_order());
}

// Test that final callback is called with success, when PNaCl does not
// look like it's already installed, but mock installer says success.
// Also check that intermediate progress events are called.
TEST_F(NaClFileHostTest, TestEnsureInstalledNotInstalledSuccess) {
  base::FilePath kTestPnaclPath;
  nacl_browser_delegate()->SetPnaclDirectory(kTestPnaclPath);
  nacl_browser_delegate()->SetShouldPnaclInstallSucceed(true);

  EnsurePnaclInstalled(
      base::Bind(&NaClFileHostTest::CallbackInstall, base::Unretained(this)),
      base::Bind(&NaClFileHostTest::CallbackProgress, base::Unretained(this)));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(install_success());
  EXPECT_EQ(1u, install_call_count());
  EXPECT_GE(progress_call_count(), 1u);
  EXPECT_TRUE(events_in_correct_order());
}

// Test that final callback is called with error, when PNaCl does not look
// like it's already installed, but mock installer says error.
// Also check that intermediate progress events are called.
TEST_F(NaClFileHostTest, TestEnsureInstalledNotInstalledError) {
  base::FilePath kTestPnaclPath;
  nacl_browser_delegate()->SetPnaclDirectory(kTestPnaclPath);
  nacl_browser_delegate()->SetShouldPnaclInstallSucceed(false);

  EnsurePnaclInstalled(
      base::Bind(&NaClFileHostTest::CallbackInstall, base::Unretained(this)),
      base::Bind(&NaClFileHostTest::CallbackProgress, base::Unretained(this)));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(install_success());
  EXPECT_EQ(1u, install_call_count());
  EXPECT_GE(progress_call_count(), 1u);
  EXPECT_TRUE(events_in_correct_order());
}
