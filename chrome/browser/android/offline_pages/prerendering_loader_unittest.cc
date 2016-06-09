// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prerendering_loader.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

// Adapter that intercepts prerender stack calls for testing.
class TestAdapter : public PrerenderAdapter {
 public:
  explicit TestAdapter(PrerenderAdapter::Observer* observer)
      : PrerenderAdapter(observer),
        active_(false),
        disabled_(false),
        observer_(observer),
        web_contents_(nullptr),
        final_status_(prerender::FinalStatus::FINAL_STATUS_MAX) {}
  ~TestAdapter() override {}

  // PrerenderAdapter implementation.
  bool CanPrerender() const override;
  bool StartPrerender(
      content::BrowserContext* browser_context,
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Size& size) override;
  content::WebContents* GetWebContents() const override;
  prerender::FinalStatus GetFinalStatus() const override;
  bool IsActive() const override;
  void DestroyActive() override;

  // Sets prerendering to be disabled. This will cause the CanPrerender()
  // to return false.
  void Disable();

  // Configures mocked prerendering details.
  void Configure(content::WebContents* web_contents,
                 prerender::FinalStatus final_status);

  // Returns the observer for test access.
  PrerenderAdapter::Observer* GetObserver() const { return observer_; }

 private:
  bool active_;
  bool disabled_;
  PrerenderAdapter::Observer* observer_;
  content::WebContents* web_contents_;
  prerender::FinalStatus final_status_;

  DISALLOW_COPY_AND_ASSIGN(TestAdapter);
};

void TestAdapter::Disable() {
  disabled_ = true;
}

void TestAdapter::Configure(content::WebContents* web_contents,
                            prerender::FinalStatus final_status) {
  web_contents_ = web_contents;
  final_status_ = final_status;
}

bool TestAdapter::CanPrerender() const {
  return !disabled_;
}

bool TestAdapter::StartPrerender(
    content::BrowserContext* browser_context,
    const GURL& url,
    content::SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size) {
  active_ = true;
  return true;
}

content::WebContents* TestAdapter::GetWebContents() const {
  return web_contents_;
}

prerender::FinalStatus TestAdapter::GetFinalStatus() const {
  return final_status_;
}

bool TestAdapter::IsActive() const {
  return active_;
}

void TestAdapter::DestroyActive() {
  active_ = false;
}

void PumpLoop() {
  base::RunLoop().RunUntilIdle();
}

}  // namespace

// Test class.
class PrerenderingLoaderTest : public testing::Test {
 public:
  PrerenderingLoaderTest();
  ~PrerenderingLoaderTest() override {}

  void SetUp() override;

  // Returns the PrerenderLoader to test.
  PrerenderingLoader* loader() const { return loader_.get(); }
  // Returns the TestAdapter to allow test behavior configuration.
  TestAdapter* test_adapter() const { return test_adapter_; }
  bool callback_called() { return callback_called_; }
  Offliner::RequestStatus callback_load_status() {
    return callback_load_status_;
  }
  Profile* profile() { return &profile_; }
  void OnLoadDone(Offliner::RequestStatus load_status,
                  content::WebContents* web_contents);

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  TestAdapter* test_adapter_;
  std::unique_ptr<PrerenderingLoader> loader_;
  bool callback_called_;
  Offliner::RequestStatus callback_load_status_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderingLoaderTest);
};

PrerenderingLoaderTest::PrerenderingLoaderTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      callback_load_status_(Offliner::RequestStatus::UNKNOWN) {}

void PrerenderingLoaderTest::SetUp() {
  loader_.reset(new PrerenderingLoader(&profile_));
  test_adapter_ = new TestAdapter(loader_.get());
  loader_->SetAdapterForTesting(base::WrapUnique(test_adapter_));
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void PrerenderingLoaderTest::OnLoadDone(Offliner::RequestStatus load_status,
                                        content::WebContents* web_contents) {
  callback_called_ = true;
  callback_load_status_ = load_status;
}

TEST_F(PrerenderingLoaderTest, CanPrerender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  EXPECT_TRUE(loader()->CanPrerender());

  test_adapter()->Disable();
  EXPECT_FALSE(loader()->CanPrerender());
}

TEST_F(PrerenderingLoaderTest, StopLoadingWhenIdle) {
  EXPECT_TRUE(loader()->IsIdle());
  loader()->StopLoading();
  EXPECT_TRUE(loader()->IsIdle());
}

TEST_F(PrerenderingLoaderTest, LoadPageLoadSucceededFromDomContentLoaded) {
  test_adapter()->Configure(
      content::WebContentsTester::CreateTestWebContents(profile(), NULL),
      prerender::FinalStatus::FINAL_STATUS_USED);
  GURL gurl("http://testit.sea");
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_FALSE(loader()->IsLoaded());
  EXPECT_TRUE(loader()->LoadPage(
      gurl,
      base::Bind(&PrerenderingLoaderTest::OnLoadDone, base::Unretained(this))));

  test_adapter()->GetObserver()->OnPrerenderDomContentLoaded();
  // Skip SnapshotController's wait time and emulate StartSnapshot call.
  loader()->StartSnapshot();
  PumpLoop();
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_TRUE(loader()->IsLoaded());
  EXPECT_TRUE(callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADED, callback_load_status());

  loader()->StopLoading();
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_FALSE(loader()->IsLoaded());
}

TEST_F(PrerenderingLoaderTest, LoadPageLoadSucceededFromPrerenderStopLoading) {
  test_adapter()->Configure(
      content::WebContentsTester::CreateTestWebContents(profile(), NULL),
      prerender::FinalStatus::FINAL_STATUS_USED);
  GURL gurl("http://testit.sea");
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_FALSE(loader()->IsLoaded());
  EXPECT_TRUE(loader()->LoadPage(
      gurl,
      base::Bind(&PrerenderingLoaderTest::OnLoadDone, base::Unretained(this))));

  test_adapter()->GetObserver()->OnPrerenderStart();
  PumpLoop();
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_FALSE(loader()->IsLoaded());

  test_adapter()->GetObserver()->OnPrerenderStopLoading();
  PumpLoop();
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_TRUE(loader()->IsLoaded());
  EXPECT_TRUE(callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADED, callback_load_status());

  // Consider Prerenderer stops (eg, times out) before Loader is done with it.
  test_adapter()->GetObserver()->OnPrerenderStop();
  PumpLoop();
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_FALSE(loader()->IsLoaded());
  EXPECT_EQ(Offliner::RequestStatus::CANCELED, callback_load_status());
}

TEST_F(PrerenderingLoaderTest, LoadPageLoadFailedNoContent) {
  test_adapter()->Configure(
      nullptr /* web_contents */,
      prerender::FinalStatus::FINAL_STATUS_MEMORY_LIMIT_EXCEEDED);
  GURL gurl("http://testit.sea");
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_TRUE(loader()->LoadPage(
      gurl,
      base::Bind(&PrerenderingLoaderTest::OnLoadDone, base::Unretained(this))));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_FALSE(loader()->IsLoaded());

  test_adapter()->GetObserver()->OnPrerenderDomContentLoaded();
  PumpLoop();
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_TRUE(callback_called());
  // We did not provide any WebContents for the callback so expect did not load.
  EXPECT_EQ(Offliner::RequestStatus::FAILED, callback_load_status());

  // Stopped event causes no harm.
  test_adapter()->GetObserver()->OnPrerenderStop();
  PumpLoop();
}

TEST_F(PrerenderingLoaderTest, LoadPageLoadCanceledFromStopLoading) {
  GURL gurl("http://testit.sea");
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_TRUE(loader()->LoadPage(
      gurl,
      base::Bind(&PrerenderingLoaderTest::OnLoadDone, base::Unretained(this))));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_FALSE(loader()->IsLoaded());

  loader()->StopLoading();
  PumpLoop();
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_TRUE(callback_called());
  EXPECT_EQ(Offliner::RequestStatus::CANCELED, callback_load_status());
}

TEST_F(PrerenderingLoaderTest, LoadPageNotAcceptedWhenNotIdle) {
  GURL gurl("http://testit.sea");
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_TRUE(loader()->LoadPage(
      gurl,
      base::Bind(&PrerenderingLoaderTest::OnLoadDone, base::Unretained(this))));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_FALSE(loader()->IsLoaded());

  // Now try another load while first is still active.
  EXPECT_FALSE(loader()->LoadPage(
      gurl,
      base::Bind(&PrerenderingLoaderTest::OnLoadDone, base::Unretained(this))));
  EXPECT_FALSE(loader()->IsIdle());
}

TEST_F(PrerenderingLoaderTest, LoadPageNotAcceptedWhenPrerenderingDisabled) {
  test_adapter()->Disable();
  GURL gurl("http://testit.sea");
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_FALSE(loader()->LoadPage(
      gurl,
      base::Bind(&PrerenderingLoaderTest::OnLoadDone, base::Unretained(this))));
  EXPECT_TRUE(loader()->IsIdle());
}

}  // namespace offline_pages
