// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prerender_adapter.h"

#include "base/sys_info.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using prerender::FinalStatus;
using prerender::Origin;
using prerender::PrerenderContents;
using prerender::PrerenderManager;
using prerender::PrerenderManagerFactory;

namespace offline_pages {

namespace {

class StubPrerenderContents : public PrerenderContents {
 public:
  StubPrerenderContents(PrerenderManager* prerender_manager,
                        const GURL& url,
                        Origin origin);

  ~StubPrerenderContents() override;

  void StartPrerendering(
      const gfx::Rect& bounds,
      content::SessionStorageNamespace* session_storage_namespace) override;

  void ReportStartEvent() { NotifyPrerenderStart(); }
  void ReportOnLoadEvent() { NotifyPrerenderStopLoading(); }
  void ReportDomContentEvent() { NotifyPrerenderDomContentLoaded(); }
  void StopWithStatus(FinalStatus final_status) { Destroy(final_status); }
};

StubPrerenderContents::StubPrerenderContents(
    PrerenderManager* prerender_manager,
    const GURL& url,
    Origin origin)
    : PrerenderContents(prerender_manager,
                        NULL,
                        url,
                        content::Referrer(),
                        origin) {}

StubPrerenderContents::~StubPrerenderContents() {}

void StubPrerenderContents::StartPrerendering(
    const gfx::Rect& bounds,
    content::SessionStorageNamespace* session_storage_namespace) {
  prerendering_has_started_ = true;
}

class StubPrerenderContentsFactory : public PrerenderContents::Factory {
 public:
  StubPrerenderContentsFactory()
      : create_prerender_contents_called_(false),
        last_prerender_contents_(nullptr) {}

  ~StubPrerenderContentsFactory() override {}

  PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager,
      Profile* profile,
      const GURL& url,
      const content::Referrer& referrer,
      Origin origin) override;

  bool create_prerender_contents_called() const {
    return create_prerender_contents_called_;
  }

  StubPrerenderContents* last_prerender_contents() const {
    return last_prerender_contents_;
  }

 private:
  bool create_prerender_contents_called_;
  StubPrerenderContents* last_prerender_contents_;

  DISALLOW_COPY_AND_ASSIGN(StubPrerenderContentsFactory);
};

PrerenderContents* StubPrerenderContentsFactory::CreatePrerenderContents(
    PrerenderManager* prerender_manager,
    Profile* profile,
    const GURL& url,
    const content::Referrer& referrer,
    Origin origin) {
  create_prerender_contents_called_ = true;
  last_prerender_contents_ = new StubPrerenderContents(
      prerender_manager, url, prerender::ORIGIN_OFFLINE);
  return last_prerender_contents_;
}

}  // namespace

// Test class.
class PrerenderAdapterTest : public testing::Test,
                             public PrerenderAdapter::Observer {
 public:
  PrerenderAdapterTest();
  ~PrerenderAdapterTest() override;

  // PrerenderAdapter::Observer implementation:
  void OnPrerenderStopLoading() override;
  void OnPrerenderDomContentLoaded() override;
  void OnPrerenderStop() override;
  void OnPrerenderNetworkBytesChanged(int64_t bytes) override;

  void SetUp() override;

  // Returns the PrerenderLoader to test.
  PrerenderAdapter* adapter() const { return adapter_.get(); }
  StubPrerenderContentsFactory* prerender_contents_factory() {
    return prerender_contents_factory_;
  }
  Profile* profile() { return &profile_; }
  PrerenderManager* prerender_manager() { return prerender_manager_; }
  bool observer_stop_loading_called() const {
    return observer_stop_loading_called_;
  }
  bool observer_dom_content_loaded_called() const {
    return observer_dom_content_loaded_called_;
  }
  bool observer_stop_called() const { return observer_stop_called_; }
  int64_t observer_network_bytes_changed() const {
    return observer_network_bytes_changed_;
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  std::unique_ptr<PrerenderAdapter> adapter_;
  StubPrerenderContentsFactory* prerender_contents_factory_;
  PrerenderManager* prerender_manager_;
  bool observer_stop_loading_called_;
  bool observer_dom_content_loaded_called_;
  bool observer_stop_called_;
  int64_t observer_network_bytes_changed_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderAdapterTest);
};

PrerenderAdapterTest::PrerenderAdapterTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      prerender_manager_(nullptr),
      observer_stop_loading_called_(false),
      observer_dom_content_loaded_called_(false),
      observer_stop_called_(false),
      observer_network_bytes_changed_(0) {}

PrerenderAdapterTest::~PrerenderAdapterTest() {
  if (prerender_manager_)
    prerender_manager_->Shutdown();
}

void PrerenderAdapterTest::OnPrerenderStopLoading() {
  observer_stop_loading_called_ = true;
}

void PrerenderAdapterTest::OnPrerenderDomContentLoaded() {
  observer_dom_content_loaded_called_ = true;
}

void PrerenderAdapterTest::OnPrerenderStop() {
  observer_stop_called_ = true;
}

void PrerenderAdapterTest::OnPrerenderNetworkBytesChanged(int64_t bytes) {
  observer_network_bytes_changed_ = bytes;
}

void PrerenderAdapterTest::SetUp() {
  if (base::SysInfo::IsLowEndDevice())
    return;
  adapter_.reset(new PrerenderAdapter(this));
  prerender_contents_factory_ = new StubPrerenderContentsFactory();
  prerender_manager_ = PrerenderManagerFactory::GetForBrowserContext(profile());
  if (prerender_manager_) {
    prerender_manager_->SetPrerenderContentsFactoryForTest(
        prerender_contents_factory_);
    prerender_manager_->SetMode(PrerenderManager::PRERENDER_MODE_ENABLED);
  }
  observer_stop_loading_called_ = false;
  observer_dom_content_loaded_called_ = false;
  observer_stop_called_ = false;
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

TEST_F(PrerenderAdapterTest, StartPrerenderFailsForUnsupportedScheme) {
  // Skip test on low end device until supported.
  if (base::SysInfo::IsLowEndDevice())
    return;

  std::unique_ptr<content::WebContents> session_contents(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())));
  content::SessionStorageNamespace* sessionStorageNamespace =
      session_contents->GetController().GetDefaultSessionStorageNamespace();
  gfx::Size renderWindowSize = session_contents->GetContainerBounds().size();
  GURL url("file://file.test");
  EXPECT_FALSE(adapter()->StartPrerender(
      profile(), url, sessionStorageNamespace, renderWindowSize));
  EXPECT_TRUE(prerender_contents_factory()->create_prerender_contents_called());
  EXPECT_FALSE(adapter()->IsActive());
}

TEST_F(PrerenderAdapterTest, StartPrerenderSucceeds) {
  // Skip test on low end device until supported.
  if (base::SysInfo::IsLowEndDevice())
    return;

  std::unique_ptr<content::WebContents> session_contents(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())));
  content::SessionStorageNamespace* sessionStorageNamespace =
      session_contents->GetController().GetDefaultSessionStorageNamespace();
  gfx::Size renderWindowSize = session_contents->GetContainerBounds().size();
  GURL url("https://url.test");
  EXPECT_TRUE(adapter()->StartPrerender(profile(), url, sessionStorageNamespace,
                                        renderWindowSize));
  EXPECT_TRUE(prerender_contents_factory()->create_prerender_contents_called());
  EXPECT_NE(nullptr, prerender_contents_factory()->last_prerender_contents());
  EXPECT_TRUE(adapter()->IsActive());
  EXPECT_FALSE(observer_stop_loading_called());
  EXPECT_FALSE(observer_dom_content_loaded_called());
  EXPECT_FALSE(observer_stop_called());

  // Exercise observer event call paths.
  prerender_contents_factory()
      ->last_prerender_contents()
      ->ReportDomContentEvent();
  EXPECT_TRUE(observer_dom_content_loaded_called());

  // Expect byte count reported to Observer.
  prerender_contents_factory()->last_prerender_contents()->AddNetworkBytes(153);
  EXPECT_EQ(153LL, observer_network_bytes_changed());

  prerender_contents_factory()->last_prerender_contents()->ReportOnLoadEvent();
  EXPECT_TRUE(observer_stop_loading_called());
  prerender_contents_factory()->last_prerender_contents()->StopWithStatus(
      FinalStatus::FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(observer_stop_called());
  EXPECT_EQ(FinalStatus::FINAL_STATUS_CANCELLED, adapter()->GetFinalStatus());

  // Exercise access methods even though no interesting values set beneath.
  EXPECT_EQ(nullptr, adapter()->GetWebContents());

  adapter()->DestroyActive();
  EXPECT_FALSE(adapter()->IsActive());
}

}  // namespace offline_pages
