// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace prerender {

namespace {

class DummyPrerenderContents : public PrerenderContents {
 public:
  DummyPrerenderContents(PrerenderManager* prerender_manager,
                         const GURL& url,
                         FinalStatus expected_final_status)
      : PrerenderContents(prerender_manager, NULL, url,
                          std::vector<GURL>(), GURL()),
        has_started_(false),
        expected_final_status_(expected_final_status) {
  }

  DummyPrerenderContents(PrerenderManager* prerender_manager,
                         const GURL& url,
                         const std::vector<GURL> alias_urls,
                         FinalStatus expected_final_status)
      : PrerenderContents(prerender_manager, NULL, url, alias_urls, GURL()),
        has_started_(false),
        expected_final_status_(expected_final_status) {
  }

  virtual ~DummyPrerenderContents() {
    EXPECT_EQ(expected_final_status_, final_status());
  }

  virtual void StartPrerendering() OVERRIDE {
    has_started_ = true;
  }

  virtual bool GetChildId(int* child_id) const OVERRIDE {
    *child_id = 0;
    return true;
  }

  virtual bool GetRouteId(int* route_id) const OVERRIDE {
    *route_id = 0;
    return true;
  }

  bool has_started() const { return has_started_; }

 private:
  bool has_started_;
  FinalStatus expected_final_status_;
};

class TestPrerenderManager : public PrerenderManager {
 public:
  TestPrerenderManager()
      : PrerenderManager(NULL),
        time_(base::Time::Now()),
        time_ticks_(base::TimeTicks::Now()),
        next_pc_(NULL) {
    rate_limit_enabled_ = false;
  }

  void AdvanceTime(base::TimeDelta delta) {
    time_ += delta;
  }

  void AdvanceTimeTicks(base::TimeDelta delta) {
    time_ticks_ += delta;
  }

  void SetNextPrerenderContents(PrerenderContents* pc) {
    next_pc_.reset(pc);
  }

  // Shorthand to add a simple preload with no aliases.
  bool AddSimplePreload(const GURL& url) {
    return AddPreload(url, std::vector<GURL>(), GURL());
  }

  bool IsPendingEntry(const GURL& url) {
    return (PrerenderManager::FindPendingEntry(url) != NULL);
  }

  void set_rate_limit_enabled(bool enabled) { rate_limit_enabled_ = true; }

  PrerenderContents* next_pc() { return next_pc_.get(); }

 protected:
  virtual ~TestPrerenderManager() {
    if (next_pc()) {
      next_pc()->set_final_status(
          FINAL_STATUS_MANAGER_SHUTDOWN);
    }
  }

 private:
  virtual base::Time GetCurrentTime() const OVERRIDE {
    return time_;
  }

  virtual base::TimeTicks GetCurrentTimeTicks() const OVERRIDE {
    return time_ticks_;
  }

  virtual PrerenderContents* CreatePrerenderContents(
      const GURL& url,
      const std::vector<GURL>& alias_urls,
      const GURL& referrer) OVERRIDE {
    DCHECK(next_pc_.get());
    return next_pc_.release();
  }

  base::Time time_;
  base::TimeTicks time_ticks_;
  scoped_ptr<PrerenderContents> next_pc_;
};

}  // namespace

class PrerenderManagerTest : public testing::Test {
 public:
  PrerenderManagerTest() : prerender_manager_(new TestPrerenderManager()),
                           ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 protected:
  scoped_refptr<TestPrerenderManager> prerender_manager_;

 private:
  // Needed to pass PrerenderManager's DCHECKs.
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
};

TEST_F(PrerenderManagerTest, EmptyTest) {
  GURL url("http://www.google.com/");
  EXPECT_FALSE(prerender_manager_->MaybeUsePreloadedPage(NULL, url));
}

TEST_F(PrerenderManagerTest, FoundTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* pc =
      new DummyPrerenderContents(prerender_manager_.get(),
                                 url,
                                 FINAL_STATUS_USED);
  prerender_manager_->SetNextPrerenderContents(pc);
  EXPECT_TRUE(prerender_manager_->AddSimplePreload(url));
  EXPECT_TRUE(pc->has_started());
  ASSERT_EQ(pc, prerender_manager_->GetEntry(url));
  pc->set_final_status(FINAL_STATUS_USED);
  delete pc;
}

// Make sure that if queue a request, and a second prerender request for the
// same URL comes in, that we drop the second request and keep the first one.
TEST_F(PrerenderManagerTest, DropSecondRequestTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* pc =
      new DummyPrerenderContents(prerender_manager_.get(), url,
                                 FINAL_STATUS_USED);
  DummyPrerenderContents* null = NULL;
  prerender_manager_->SetNextPrerenderContents(pc);
  EXPECT_TRUE(prerender_manager_->AddSimplePreload(url));
  EXPECT_EQ(null, prerender_manager_->next_pc());
  EXPECT_TRUE(pc->has_started());
  DummyPrerenderContents* pc1 =
      new DummyPrerenderContents(
          prerender_manager_.get(), url,
          FINAL_STATUS_MANAGER_SHUTDOWN);
  prerender_manager_->SetNextPrerenderContents(pc1);
  EXPECT_FALSE(prerender_manager_->AddSimplePreload(url));
  EXPECT_EQ(pc1, prerender_manager_->next_pc());
  EXPECT_FALSE(pc1->has_started());
  ASSERT_EQ(pc, prerender_manager_->GetEntry(url));
  pc->set_final_status(FINAL_STATUS_USED);
  delete pc;
}

// Ensure that we expire a prerendered page after the max. permitted time.
TEST_F(PrerenderManagerTest, ExpireTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* pc =
      new DummyPrerenderContents(prerender_manager_.get(), url,
                                 FINAL_STATUS_TIMED_OUT);
  DummyPrerenderContents* null = NULL;
  prerender_manager_->SetNextPrerenderContents(pc);
  EXPECT_TRUE(prerender_manager_->AddSimplePreload(url));
  EXPECT_EQ(null, prerender_manager_->next_pc());
  EXPECT_TRUE(pc->has_started());
  prerender_manager_->AdvanceTime(prerender_manager_->max_prerender_age()
                                  + base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(null, prerender_manager_->GetEntry(url));
}

// LRU Test.  Make sure that if we prerender more than one request, that
// the oldest one will be dropped.
TEST_F(PrerenderManagerTest, DropOldestRequestTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* pc =
      new DummyPrerenderContents(prerender_manager_.get(), url,
                                 FINAL_STATUS_EVICTED);
  DummyPrerenderContents* null = NULL;
  prerender_manager_->SetNextPrerenderContents(pc);
  EXPECT_TRUE(prerender_manager_->AddSimplePreload(url));
  EXPECT_EQ(null, prerender_manager_->next_pc());
  EXPECT_TRUE(pc->has_started());
  GURL url1("http://news.google.com/");
  DummyPrerenderContents* pc1 =
      new DummyPrerenderContents(prerender_manager_.get(), url1,
                                 FINAL_STATUS_USED);
  prerender_manager_->SetNextPrerenderContents(pc1);
  EXPECT_TRUE(prerender_manager_->AddSimplePreload(url1));
  EXPECT_EQ(null, prerender_manager_->next_pc());
  EXPECT_TRUE(pc1->has_started());
  ASSERT_EQ(null, prerender_manager_->GetEntry(url));
  ASSERT_EQ(pc1, prerender_manager_->GetEntry(url1));
  pc1->set_final_status(FINAL_STATUS_USED);
  delete pc1;
}

// Two element prerender test.  Ensure that the LRU operates correctly if we
// permit 2 elements to be kept prerendered.
TEST_F(PrerenderManagerTest, TwoElementPrerenderTest) {
  prerender_manager_->set_max_elements(2);
  GURL url("http://www.google.com/");
  DummyPrerenderContents* pc =
      new DummyPrerenderContents(prerender_manager_.get(), url,
                                 FINAL_STATUS_EVICTED);
  DummyPrerenderContents* null = NULL;
  prerender_manager_->SetNextPrerenderContents(pc);
  EXPECT_TRUE(prerender_manager_->AddSimplePreload(url));
  EXPECT_EQ(null, prerender_manager_->next_pc());
  EXPECT_TRUE(pc->has_started());
  GURL url1("http://news.google.com/");
  DummyPrerenderContents* pc1 =
      new DummyPrerenderContents(prerender_manager_.get(),  url1,
                                 FINAL_STATUS_USED);
  prerender_manager_->SetNextPrerenderContents(pc1);
  EXPECT_TRUE(prerender_manager_->AddSimplePreload(url1));
  EXPECT_EQ(null, prerender_manager_->next_pc());
  EXPECT_TRUE(pc1->has_started());
  GURL url2("http://images.google.com/");
  DummyPrerenderContents* pc2 =
      new DummyPrerenderContents(prerender_manager_.get(), url2,
                                 FINAL_STATUS_USED);
  prerender_manager_->SetNextPrerenderContents(pc2);
  EXPECT_TRUE(prerender_manager_->AddSimplePreload(url2));
  EXPECT_EQ(null, prerender_manager_->next_pc());
  EXPECT_TRUE(pc2->has_started());
  ASSERT_EQ(null, prerender_manager_->GetEntry(url));
  ASSERT_EQ(pc1, prerender_manager_->GetEntry(url1));
  ASSERT_EQ(pc2, prerender_manager_->GetEntry(url2));
  pc1->set_final_status(FINAL_STATUS_USED);
  delete pc1;
  pc2->set_final_status(FINAL_STATUS_USED);
  delete pc2;
}

TEST_F(PrerenderManagerTest, AliasURLTest) {
  GURL url("http://www.google.com/");
  GURL alias_url1("http://www.google.com/index.html");
  GURL alias_url2("http://google.com/");
  GURL not_an_alias_url("http://google.com/index.html");
  std::vector<GURL> alias_urls;
  alias_urls.push_back(alias_url1);
  alias_urls.push_back(alias_url2);
  DummyPrerenderContents* pc =
      new DummyPrerenderContents(prerender_manager_.get(), url, alias_urls,
                                 FINAL_STATUS_USED);
  // Test that all of the aliases work, but nont_an_alias_url does not.
  prerender_manager_->SetNextPrerenderContents(pc);
  EXPECT_TRUE(prerender_manager_->AddPreload(url, alias_urls, GURL()));
  ASSERT_EQ(NULL, prerender_manager_->GetEntry(not_an_alias_url));
  ASSERT_EQ(pc, prerender_manager_->GetEntry(alias_url1));
  prerender_manager_->SetNextPrerenderContents(pc);
  EXPECT_TRUE(prerender_manager_->AddPreload(url, alias_urls, GURL()));
  ASSERT_EQ(pc, prerender_manager_->GetEntry(alias_url2));
  prerender_manager_->SetNextPrerenderContents(pc);
  EXPECT_TRUE(prerender_manager_->AddPreload(url, alias_urls, GURL()));
  ASSERT_EQ(pc, prerender_manager_->GetEntry(url));

  // Test that alias URLs can not be added.
  prerender_manager_->SetNextPrerenderContents(pc);
  EXPECT_TRUE(prerender_manager_->AddPreload(url, alias_urls, GURL()));
  EXPECT_FALSE(prerender_manager_->AddSimplePreload(url));
  EXPECT_FALSE(prerender_manager_->AddSimplePreload(alias_url1));
  EXPECT_FALSE(prerender_manager_->AddSimplePreload(alias_url2));
  ASSERT_EQ(pc, prerender_manager_->GetEntry(url));

  pc->set_final_status(FINAL_STATUS_USED);
  delete pc;
}

// Ensure that we ignore prerender requests within the rate limit.
TEST_F(PrerenderManagerTest, RateLimitInWindowTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* pc =
      new DummyPrerenderContents(prerender_manager_.get(), url,
                                 FINAL_STATUS_MANAGER_SHUTDOWN);
  DummyPrerenderContents* null = NULL;
  prerender_manager_->SetNextPrerenderContents(pc);
  EXPECT_TRUE(prerender_manager_->AddSimplePreload(url));
  EXPECT_EQ(null, prerender_manager_->next_pc());
  EXPECT_TRUE(pc->has_started());

  prerender_manager_->set_rate_limit_enabled(true);
  prerender_manager_->AdvanceTimeTicks(base::TimeDelta::FromMilliseconds(1));

  GURL url1("http://news.google.com/");
  DummyPrerenderContents* rate_limit_pc =
      new DummyPrerenderContents(prerender_manager_.get(), url1,
                                 FINAL_STATUS_MANAGER_SHUTDOWN);
  prerender_manager_->SetNextPrerenderContents(rate_limit_pc);
  EXPECT_FALSE(prerender_manager_->AddSimplePreload(url1));
  prerender_manager_->set_rate_limit_enabled(false);
}

// Ensure that we don't ignore prerender requests outside the rate limit.
TEST_F(PrerenderManagerTest, RateLimitOutsideWindowTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* pc =
      new DummyPrerenderContents(prerender_manager_.get(), url,
                                 FINAL_STATUS_EVICTED);
  DummyPrerenderContents* null = NULL;
  prerender_manager_->SetNextPrerenderContents(pc);
  EXPECT_TRUE(prerender_manager_->AddSimplePreload(url));
  EXPECT_EQ(null, prerender_manager_->next_pc());
  EXPECT_TRUE(pc->has_started());

  prerender_manager_->set_rate_limit_enabled(true);
  prerender_manager_->AdvanceTimeTicks(base::TimeDelta::FromMilliseconds(2000));

  GURL url1("http://news.google.com/");
  DummyPrerenderContents* rate_limit_pc =
      new DummyPrerenderContents(prerender_manager_.get(), url1,
                                 FINAL_STATUS_MANAGER_SHUTDOWN);
  prerender_manager_->SetNextPrerenderContents(rate_limit_pc);
  EXPECT_TRUE(prerender_manager_->AddSimplePreload(url1));
  EXPECT_EQ(null, prerender_manager_->next_pc());
  EXPECT_TRUE(rate_limit_pc->has_started());
  prerender_manager_->set_rate_limit_enabled(false);
}

TEST_F(PrerenderManagerTest, PendingPreloadTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* pc =
      new DummyPrerenderContents(prerender_manager_.get(),
                                 url,
                                 FINAL_STATUS_USED);
  prerender_manager_->SetNextPrerenderContents(pc);
  EXPECT_TRUE(prerender_manager_->AddSimplePreload(url));

  int child_id;
  int route_id;
  ASSERT_TRUE(pc->GetChildId(&child_id));
  ASSERT_TRUE(pc->GetRouteId(&route_id));

  GURL pending_url("http://news.google.com/");

  prerender_manager_->AddPendingPreload(std::make_pair(child_id, route_id),
                                        pending_url,
                                        std::vector<GURL>(),
                                        url);

  EXPECT_TRUE(prerender_manager_->IsPendingEntry(pending_url));
  EXPECT_TRUE(pc->has_started());
  ASSERT_EQ(pc, prerender_manager_->GetEntry(url));
  pc->set_final_status(FINAL_STATUS_USED);

  delete pc;
}

TEST_F(PrerenderManagerTest, PendingPreloadSkippedTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* pc =
      new DummyPrerenderContents(prerender_manager_.get(),
                                 url,
                                 FINAL_STATUS_TIMED_OUT);
  prerender_manager_->SetNextPrerenderContents(pc);

  int child_id;
  int route_id;
  ASSERT_TRUE(pc->GetChildId(&child_id));
  ASSERT_TRUE(pc->GetRouteId(&route_id));

  EXPECT_TRUE(prerender_manager_->AddSimplePreload(url));
  prerender_manager_->AdvanceTime(prerender_manager_->max_prerender_age()
                                  + base::TimeDelta::FromSeconds(1));
  // GetEntry will cull old entries which should now include pc.
  ASSERT_EQ(NULL, prerender_manager_->GetEntry(url));

  GURL pending_url("http://news.google.com/");

  prerender_manager_->AddPendingPreload(std::make_pair(child_id, route_id),
                                        pending_url,
                                        std::vector<GURL>(),
                                        url);
  EXPECT_FALSE(prerender_manager_->IsPendingEntry(pending_url));
}

// Ensure that extracting a urlencoded URL in the url= query string component
// works.
TEST_F(PrerenderManagerTest, ExtractURLInQueryStringTest) {
  GURL result;
  EXPECT_TRUE(PrerenderManager::MaybeGetQueryStringBasedAliasURL(
      GURL("http://www.google.com/url?sa=t&source=web&cd=1&ved=0CBcQFjAA&url=http%3A%2F%2Fwww.abercrombie.com%2Fwebapp%2Fwcs%2Fstores%2Fservlet%2FStoreLocator%3FcatalogId%3D%26storeId%3D10051%26langId%3D-1&rct=j&q=allinurl%3A%26&ei=KLyUTYGSEdTWiAKUmLCdCQ&usg=AFQjCNF8nJ2MpBFfr1ijO39_f22bcKyccw&sig2=2ymyGpO0unJwU1d4kdCUjQ"),
      &result));
  ASSERT_EQ(GURL("http://www.abercrombie.com/webapp/wcs/stores/servlet/StoreLocator?catalogId=&storeId=10051&langId=-1").spec(), result.spec());
  EXPECT_FALSE(PrerenderManager::MaybeGetQueryStringBasedAliasURL(
      GURL("http://www.google.com/url?sadf=test&blah=blahblahblah"), &result));
  EXPECT_FALSE(PrerenderManager::MaybeGetQueryStringBasedAliasURL(
      GURL("http://www.google.com/?url=INVALIDurlsAREsoMUCHfun.com"), &result));
  EXPECT_TRUE(PrerenderManager::MaybeGetQueryStringBasedAliasURL(
      GURL("http://www.google.com/?url=http://validURLSareGREAT.com"),
      &result));
  ASSERT_EQ(GURL("http://validURLSareGREAT.com").spec(), result.spec());
}

}  // namespace prerender
