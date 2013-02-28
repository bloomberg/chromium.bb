// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_cycler/page_cycler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using content::RenderViewHost;
using content::TestBrowserThread;
using content::WebContentsObserver;
using file_util::ContentsEqual;
using file_util::PathExists;

namespace {
const int kFrameID = 1;
const bool kIsMainFrame = true;
const GURL kAboutURL = GURL(chrome::kAboutBlankURL);
}  // namespace

class MockPageCycler : public PageCycler {
 public:
  MockPageCycler(Browser* browser, base::FilePath urls_file,
                 base::FilePath errors_file)
      : PageCycler(browser, urls_file) {
    set_errors_file(errors_file);
  }

  MockPageCycler(Browser* browser,
                 base::FilePath urls_file,
                 base::FilePath errors_file,
                 base::FilePath stats_file)
      : PageCycler(browser, urls_file) {
    set_stats_file(stats_file);
    set_errors_file(errors_file);
  }

  MOCK_METHOD4(DidFinishLoad, void(int64 frame_id,
                                   const GURL& validated_url,
                                   bool is_main_frame,
                                   RenderViewHost* render_view_host));
  MOCK_METHOD6(DidFailProvisionalLoad, void(int64 frame_id,
                                            bool is_main_frame,
                                            const GURL& validated_url,
                                            int error_code,
                                            const string16& error_description,
                                            RenderViewHost* render_view_host));
  MOCK_METHOD1(RenderViewGone, void(base::TerminationStatus status));

  void PageCyclerDidFailProvisionalLoad(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      int error_code,
      const string16& error_description,
      RenderViewHost* render_view_host) {
    PageCycler::DidFailProvisionalLoad(frame_id, is_main_frame,
                                       validated_url,
                                       error_code, error_description,
                                       render_view_host);
  }

  void PageCyclerDidFinishLoad(int64 frame_id,
                               const GURL& validated_url,
                               bool is_main_frame,
                               RenderViewHost* render_view_host) {
    PageCycler::DidFinishLoad(
        frame_id, validated_url, is_main_frame, render_view_host);
  }

 private:
  // We need to override Finish() because the calls to exit the browser in a
  // real PageCycler do not work in unittests (they interfere with later tests).
  virtual void Finish() OVERRIDE {
    BrowserList::RemoveObserver(this);
    Release();
  }

  virtual ~MockPageCycler() {}\

  DISALLOW_COPY_AND_ASSIGN(MockPageCycler);
};

class PageCyclerTest : public BrowserWithTestWindowTest {
 public:
  PageCyclerTest() {
  }

  virtual ~PageCyclerTest() {
  }

  virtual void SetUp() OVERRIDE {
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
    test_data_dir_ = test_data_dir_.AppendASCII("page_cycler");

    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), kAboutURL);
    ASSERT_FALSE(browser()->tab_strip_model()->GetActiveWebContents() == NULL);
  }

  void InitFilePaths(const base::FilePath& temp_path) {
    errors_file_ = temp_path.AppendASCII("errors_file");
    stats_file_ = temp_path.AppendASCII("stats_file");

    CHECK(!file_util::PathExists(errors_file_));
    CHECK(!file_util::PathExists(stats_file_));
  }

  void FailProvisionalLoad(int error_code, string16& error_description) {
    FOR_EACH_OBSERVER(
        WebContentsObserver,
        observers_,
        DidFailProvisionalLoad(kFrameID, kIsMainFrame, kAboutURL, error_code,
                               error_description, NULL));
    PumpLoop();
  }

  void FinishLoad() {
    FOR_EACH_OBSERVER(
        WebContentsObserver,
        observers_,
        DidFinishLoad(kFrameID, kAboutURL, kIsMainFrame, NULL));
    PumpLoop();
  }

  void RunPageCycler() {
    page_cycler_->Run();
    PumpLoop();
  }

  void PumpLoop() {
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    message_loop()->RunUntilIdle();
  }

  void CloseBrowser() {
    DestroyBrowserAndProfile();
    PumpLoop();
  }

  MockPageCycler* page_cycler() {
    return page_cycler_.get();
  }

  void set_page_cycler(MockPageCycler* page_cycler) {
    page_cycler_ = page_cycler;
    observers_.AddObserver(page_cycler);
  }

  const std::vector<GURL>* urls_for_test() {
    return page_cycler_->urls_for_test();
  }

  base::FilePath stats_file() {
    return stats_file_;
  }

  base::FilePath errors_file() {
    return errors_file_;
  }

  base::FilePath urls_file() {
    return test_data_dir_.AppendASCII("about_url");
  }

  base::FilePath test_data_dir() {
    return test_data_dir_;
  }

 private:
  ObserverList<WebContentsObserver> observers_;
  scoped_refptr<MockPageCycler> page_cycler_;
  base::FilePath test_data_dir_;
  base::FilePath stats_file_;
  base::FilePath errors_file_;
  base::FilePath urls_file_;
};

TEST_F(PageCyclerTest, FailProvisionalLoads) {
  const base::FilePath errors_expected_file =
      test_data_dir().AppendASCII("errors_expected");

  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  InitFilePaths(temp.path());

  ASSERT_TRUE(PathExists(errors_expected_file));
  ASSERT_TRUE(PathExists(urls_file()));

  set_page_cycler(new MockPageCycler(browser(),
                                     urls_file(),
                                     errors_file()));
  RunPageCycler();

  // Page cycler expects browser to automatically start loading the first page.
  EXPECT_CALL(*page_cycler(),
              DidFinishLoad(kFrameID, kAboutURL, kIsMainFrame, _))
      .WillOnce(Invoke(page_cycler(),
                       &MockPageCycler::PageCyclerDidFinishLoad));
  FinishLoad();

  // DNS server fail error message.
  string16 error_string =
      string16(ASCIIToUTF16(net::ErrorToString(net::ERR_DNS_SERVER_FAILED)));
  EXPECT_CALL(*page_cycler(),
              DidFailProvisionalLoad(kFrameID, kIsMainFrame, _,
                                     net::ERR_DNS_SERVER_FAILED, error_string,
                                     _))
      .WillOnce(Invoke(page_cycler(),
                       &MockPageCycler::PageCyclerDidFailProvisionalLoad));
  FailProvisionalLoad(net::ERR_DNS_SERVER_FAILED, error_string);

  // DNS time-out error message.
  error_string = string16(
      ASCIIToUTF16(net::ErrorToString(net::ERR_DNS_TIMED_OUT)));
  EXPECT_CALL(*page_cycler(),
              DidFailProvisionalLoad(kFrameID,
                                     kIsMainFrame, _, net::ERR_DNS_TIMED_OUT,
                                     error_string, _))
      .WillOnce(Invoke(page_cycler(),
                       &MockPageCycler::PageCyclerDidFailProvisionalLoad));

  FailProvisionalLoad(net::ERR_DNS_TIMED_OUT, error_string);

  // DNS time-out error message.
  error_string = string16(
      ASCIIToUTF16(net::ErrorToString(net::ERR_INVALID_URL)));
  EXPECT_CALL(*page_cycler(),
              DidFailProvisionalLoad(kFrameID, kIsMainFrame, _,
                                     net::ERR_INVALID_URL, error_string, _))
      .WillOnce(Invoke(page_cycler(),
                       &MockPageCycler::PageCyclerDidFailProvisionalLoad));
  FailProvisionalLoad(net::ERR_INVALID_URL, error_string);

  PumpLoop();

  std::string errors_output;
  std::string errors_expected;
  ASSERT_TRUE(file_util::ReadFileToString(errors_file(),
                                          &errors_output));
  ASSERT_TRUE(file_util::ReadFileToString(errors_expected_file,
                                          &errors_expected));
  ASSERT_EQ(errors_output, errors_expected);
}

TEST_F(PageCyclerTest, StatsFile) {
  const int kNumLoads = 4;

  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  InitFilePaths(temp.path());

  ASSERT_TRUE(PathExists(urls_file()));

  set_page_cycler(new MockPageCycler(browser(), urls_file(),
                                     errors_file()));
  page_cycler()->set_stats_file(stats_file());
  RunPageCycler();

  for (int i = 0; i < kNumLoads; ++i) {
    EXPECT_CALL(*page_cycler(), DidFinishLoad(
        kFrameID, kAboutURL, kIsMainFrame, _))
        .WillOnce(Invoke(page_cycler(),
                         &MockPageCycler::PageCyclerDidFinishLoad));
    FinishLoad();
  }

  PumpLoop();
  EXPECT_FALSE(PathExists(errors_file()));
  ASSERT_TRUE(PathExists(stats_file()));
}

TEST_F(PageCyclerTest, KillBrowserAndAbort) {
  const base::FilePath errors_expected_file =
      test_data_dir().AppendASCII("abort_expected");

  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  InitFilePaths(temp.path());

  ASSERT_TRUE(PathExists(errors_expected_file));
  ASSERT_TRUE(PathExists(urls_file()));

  set_page_cycler(new MockPageCycler(browser(),
                                     urls_file(),
                                     errors_file()));
  RunPageCycler();

  EXPECT_CALL(*page_cycler(),
      DidFinishLoad(kFrameID, kAboutURL, kIsMainFrame, _))
      .WillOnce(Invoke(page_cycler(),
                       &MockPageCycler::PageCyclerDidFinishLoad));
  message_loop()->RunUntilIdle();

  FinishLoad();

  CloseBrowser();
  PumpLoop();

  std::string errors_output;
  std::string errors_expected;
  ASSERT_TRUE(file_util::ReadFileToString(errors_file(),
                                          &errors_output));
  ASSERT_TRUE(file_util::ReadFileToString(errors_expected_file,
                                          &errors_expected));
  ASSERT_EQ(errors_output, errors_expected);
}

TEST_F(PageCyclerTest, MultipleIterations) {
  const int kNumLoads = 4;

  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  InitFilePaths(temp.path());

  ASSERT_TRUE(PathExists(urls_file()));

  set_page_cycler(new MockPageCycler(browser(),
                                     urls_file(),
                                     errors_file()));
  page_cycler()->set_stats_file(stats_file());
  RunPageCycler();

  EXPECT_CALL(*page_cycler(),
              DidFinishLoad(kFrameID, kAboutURL, kIsMainFrame, _))
      .WillRepeatedly(Invoke(page_cycler(),
                             &MockPageCycler::PageCyclerDidFinishLoad));

  for (int i = 0; i < kNumLoads; ++i)
    FinishLoad();

  PumpLoop();
  EXPECT_FALSE(PathExists(errors_file()));
  ASSERT_TRUE(PathExists(stats_file()));
}
