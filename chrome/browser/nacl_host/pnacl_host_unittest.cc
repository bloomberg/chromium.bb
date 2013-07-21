// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/pnacl_host.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pnacl {

class PnaclHostTest : public testing::Test {
 protected:
  PnaclHostTest()
      : temp_callback_count_(0),
        thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  virtual void SetUp() {
    host_ = new PnaclHost();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    host_->InitForTest(temp_dir_.path());
    EXPECT_EQ(host_->cache_state_, PnaclHost::CacheReady);
  }
  virtual void TearDown() {
    ExpectPendingTranslations(0);
    delete host_;
  }
  // Utilities for inspecting internal state of the pnacl host
  void ExpectPendingTranslations(size_t count) {
    EXPECT_EQ(count, host_->pending_translations_.size());
  }
  void ExpectCallbackCount(int count) {
    EXPECT_EQ(count, temp_callback_count_);
  }

 public:  // Required for derived classes to bind this method
  void CallbackExpectMiss(IPC::PlatformFileForTransit fd, bool is_hit) {
    temp_callback_count_++;
    EXPECT_FALSE(is_hit);
    EXPECT_FALSE(fd == IPC::InvalidPlatformFileForTransit());
  }

 protected:
  PnaclHost* host_;
  int temp_callback_count_;
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
};

#define EXPECT_PENDING_TRANSLATIONS(n) \
  do {                                 \
    SCOPED_TRACE("");                  \
    ExpectPendingTranslations(n);      \
  } while (0)

// We don't do actual caching yet, but just return a new temp file with a miss
TEST_F(PnaclHostTest, BasicMiss) {
  nacl::PnaclCacheInfo info;
  host_->GetNexeFd(
      0,
      base::ProcessHandle(),
      0,
      0,
      info,
      base::Bind(&PnaclHostTest::CallbackExpectMiss, base::Unretained(this)));

  EXPECT_PENDING_TRANSLATIONS(1);
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_PENDING_TRANSLATIONS(1);
  ExpectCallbackCount(1);
  host_->TranslationFinished(0, 0);
  EXPECT_PENDING_TRANSLATIONS(0);
  host_->RendererClosing(0);
}

TEST_F(PnaclHostTest, BadArguments) {
  nacl::PnaclCacheInfo info;
  host_->GetNexeFd(
      0,
      base::ProcessHandle(),
      0,
      0,
      info,
      base::Bind(&PnaclHostTest::CallbackExpectMiss, base::Unretained(this)));

  EXPECT_PENDING_TRANSLATIONS(1);
  host_->TranslationFinished(0, 1);  // nonexistent translation
  EXPECT_PENDING_TRANSLATIONS(1);
  host_->RendererClosing(1);  // nonexistent renderer
  EXPECT_PENDING_TRANSLATIONS(1);
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();
  ExpectCallbackCount(1);
  host_->RendererClosing(0);  // close without finishing
  EXPECT_PENDING_TRANSLATIONS(0);
}

}  // namespace pnacl
