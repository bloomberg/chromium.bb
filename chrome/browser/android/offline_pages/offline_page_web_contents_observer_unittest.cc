// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/android/offline_pages/offline_page_web_contents_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const GURL kTestURL("http://example.com/");
const base::FilePath::CharType kTestFilePath[] = FILE_PATH_LITERAL(
    "/archive_dir/offline_page.mhtml");
const int64 kTestFileSize = 123456LL;

class TestMHTMLArchiver : public OfflinePageMHTMLArchiver {
 public:
  TestMHTMLArchiver(
      content::WebContents* web_contents,
      const GURL& url,
      const base::FilePath& archive_dir);
  ~TestMHTMLArchiver() override;

 private:
  void DoGenerateMHTML() override;

  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(TestMHTMLArchiver);
};

TestMHTMLArchiver::TestMHTMLArchiver(content::WebContents* web_contents,
                                     const GURL& url,
                                     const base::FilePath& archive_dir)
    : OfflinePageMHTMLArchiver(web_contents, archive_dir), url_(url) {
}

TestMHTMLArchiver::~TestMHTMLArchiver() {
}

void TestMHTMLArchiver::DoGenerateMHTML() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&TestMHTMLArchiver::OnGenerateMHTMLDone,
                 base::Unretained(this),
                 url_,
                 base::FilePath(kTestFilePath),
                 kTestFileSize));
}

}  // namespace

class OfflinePageWebContentsObserverTest :
    public ChromeRenderViewHostTestHarness  {
 public:
  OfflinePageWebContentsObserverTest();
  ~OfflinePageWebContentsObserverTest() override;

  void SetUp() override;
  void TearDown() override;

  // Helper to create an archiver and call its CreateArchive.
  void CreateArchive(const GURL& url);

  // Test tooling methods.
  void PumpLoop();

  OfflinePageWebContentsObserver* web_contents_observer() const {
    return web_contents_observer_;
  }

  OfflinePageArchiver::ArchiverResult last_result() const {
    return last_result_;
  }

 private:
  void OnCreateArchiveDone(OfflinePageArchiver* archiver,
                           OfflinePageArchiver::ArchiverResult result,
                           const GURL& url,
                           const base::FilePath& file_path,
                           int64 file_size);

  OfflinePageWebContentsObserver* web_contents_observer_;  // Not owned.
  scoped_ptr<TestMHTMLArchiver> archiver_;
  OfflinePageArchiver::ArchiverResult last_result_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageWebContentsObserverTest);
};

OfflinePageWebContentsObserverTest::OfflinePageWebContentsObserverTest()
    : last_result_(OfflinePageArchiver::ArchiverResult::
                       ERROR_ARCHIVE_CREATION_FAILED) {
}

OfflinePageWebContentsObserverTest::~OfflinePageWebContentsObserverTest() {
}

void OfflinePageWebContentsObserverTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
  OfflinePageWebContentsObserver::CreateForWebContents(web_contents());
  web_contents_observer_ =
      OfflinePageWebContentsObserver::FromWebContents(web_contents());
  ASSERT_FALSE(web_contents_observer_->is_document_loaded_in_main_frame());
}

void OfflinePageWebContentsObserverTest::TearDown() {
  archiver_.reset();
  content::RenderViewHostTestHarness::TearDown();
}

void OfflinePageWebContentsObserverTest::CreateArchive(const GURL& url) {
  archiver_.reset(new TestMHTMLArchiver(web_contents(), url,
                                        base::FilePath(kTestFilePath)));
  archiver_->CreateArchive(
      base::Bind(&OfflinePageWebContentsObserverTest::OnCreateArchiveDone,
                 base::Unretained(this)));
}

void OfflinePageWebContentsObserverTest::OnCreateArchiveDone(
    OfflinePageArchiver* archiver,
    OfflinePageArchiver::ArchiverResult result,
    const GURL& url,
    const base::FilePath& file_path,
    int64 file_size) {
  last_result_ = result;
}

void OfflinePageWebContentsObserverTest::PumpLoop() {
  base::RunLoop().RunUntilIdle();
}

TEST_F(OfflinePageWebContentsObserverTest, LoadMainFrameBeforeCreateArchive) {
  // Navigate in main frame. Document was not loaded yet.
  content::LoadCommittedDetails details = content::LoadCommittedDetails();
  details.is_main_frame = true;
  details.is_in_page = false;
  content::FrameNavigateParams params = content::FrameNavigateParams();
  web_contents_observer()->DidNavigateMainFrame(details, params);
  ASSERT_FALSE(web_contents_observer()->is_document_loaded_in_main_frame());

  // Load document in main frame.
  web_contents_observer()->DocumentLoadedInFrame(main_rfh());
  ASSERT_TRUE(web_contents_observer()->is_document_loaded_in_main_frame());

  // Create an archive.
  CreateArchive(kTestURL);

  PumpLoop();
  EXPECT_EQ(OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
            last_result());
}

TEST_F(OfflinePageWebContentsObserverTest, LoadMainFrameAfterCreateArchive) {
  // Navigate in main frame. Document was not loaded yet.
  content::LoadCommittedDetails details = content::LoadCommittedDetails();
  details.is_main_frame = true;
  details.is_in_page = false;
  content::FrameNavigateParams params = content::FrameNavigateParams();
  web_contents_observer()->DidNavigateMainFrame(details, params);
  ASSERT_FALSE(web_contents_observer()->is_document_loaded_in_main_frame());

  // Create an archive. Waiting for document loaded in main frame.
  CreateArchive(kTestURL);

  PumpLoop();
  EXPECT_NE(OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
            last_result());

  // Load document in main frame.
  web_contents_observer()->DocumentLoadedInFrame(main_rfh());
  ASSERT_TRUE(web_contents_observer()->is_document_loaded_in_main_frame());

  PumpLoop();
  EXPECT_EQ(OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
            last_result());
}

TEST_F(OfflinePageWebContentsObserverTest, ResetOnPageNavigation) {
  // Navigate in main frame. Document was not loaded yet.
  content::LoadCommittedDetails details = content::LoadCommittedDetails();
  details.is_main_frame = true;
  details.is_in_page = false;
  content::FrameNavigateParams params = content::FrameNavigateParams();
  web_contents_observer()->DidNavigateMainFrame(details, params);
  ASSERT_FALSE(web_contents_observer()->is_document_loaded_in_main_frame());

  // Load document in main frame.
  web_contents_observer()->DocumentLoadedInFrame(main_rfh());
  ASSERT_TRUE(web_contents_observer()->is_document_loaded_in_main_frame());

  // Another navigation should result in waiting for a page load.
  web_contents_observer()->DidNavigateMainFrame(details, params);
  ASSERT_FALSE(web_contents_observer()->is_document_loaded_in_main_frame());

  // Create an archive. Waiting for document loaded in main frame.
  CreateArchive(kTestURL);

  PumpLoop();
  EXPECT_NE(OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
            last_result());

  // Load document in main frame.
  web_contents_observer()->DocumentLoadedInFrame(main_rfh());
  ASSERT_TRUE(web_contents_observer()->is_document_loaded_in_main_frame());

  PumpLoop();
  EXPECT_EQ(OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
            last_result());
}

}  // namespace offline_pages
