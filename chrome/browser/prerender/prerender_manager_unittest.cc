// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace prerender {

namespace {

class DummyPrerenderContents : public PrerenderContents {
 public:
  DummyPrerenderContents(PrerenderManager* prerender_manager,
                         PrerenderTracker* prerender_tracker,
                         const GURL& url,
                         FinalStatus expected_final_status)
      : PrerenderContents(prerender_manager, prerender_tracker,
                          NULL, url, GURL(), ORIGIN_LINK_REL_PRERENDER,
                          PrerenderManager::kNoExperiment),
        has_started_(false),
        expected_final_status_(expected_final_status) {
  }

  virtual ~DummyPrerenderContents() {
    EXPECT_EQ(expected_final_status_, final_status());
  }

  virtual void StartPrerendering(
      const RenderViewHost* source_render_view_host,
      SessionStorageNamespace* session_storage_namespace) OVERRIDE {
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

  FinalStatus expected_final_status() const { return expected_final_status_; }

 private:
  bool has_started_;
  FinalStatus expected_final_status_;
};

class TestPrerenderManager : public PrerenderManager {
 public:
  explicit TestPrerenderManager(PrerenderTracker* prerender_tracker)
      : PrerenderManager(NULL, prerender_tracker),
        time_(base::Time::Now()),
        time_ticks_(base::TimeTicks::Now()),
        next_prerender_contents_(NULL),
        prerender_tracker_(prerender_tracker) {
    set_rate_limit_enabled(false);
  }

  virtual ~TestPrerenderManager() {
    if (next_prerender_contents()) {
      next_prerender_contents_.release()->Destroy(
          FINAL_STATUS_MANAGER_SHUTDOWN);
    }
    // Set the final status for all PrerenderContents with an expected final
    // status of FINAL_STATUS_USED.  These values are normally set when the
    // prerendered RVH is swapped into a tab, which doesn't happen in these
    // unit tests.
    for (ScopedVector<PrerenderContents>::iterator it =
             used_prerender_contents_.begin();
         it != used_prerender_contents_.end(); ++it) {
      (*it)->set_final_status(FINAL_STATUS_USED);
    }
    DoShutdown();
  }

  void AdvanceTime(base::TimeDelta delta) {
    time_ += delta;
  }

  void AdvanceTimeTicks(base::TimeDelta delta) {
    time_ticks_ += delta;
  }

  DummyPrerenderContents* CreateNextPrerenderContents(
      const GURL& url,
      FinalStatus expected_final_status) {
    DummyPrerenderContents* prerender_contents =
        new DummyPrerenderContents(this, prerender_tracker_, url,
                                   expected_final_status);
    SetNextPrerenderContents(prerender_contents);
    return prerender_contents;
  }

  DummyPrerenderContents* CreateNextPrerenderContents(
      const GURL& url,
      const std::vector<GURL>& alias_urls,
      FinalStatus expected_final_status) {
    DummyPrerenderContents* prerender_contents =
        new DummyPrerenderContents(this, prerender_tracker_, url,
                                   expected_final_status);
    for (std::vector<GURL>::const_iterator it = alias_urls.begin();
         it != alias_urls.end();
         ++it) {
      EXPECT_TRUE(prerender_contents->AddAliasURL(*it));
    }
    SetNextPrerenderContents(prerender_contents);
    return prerender_contents;
  }

  // Shorthand to add a simple preload with a reasonable source.
  bool AddSimplePrerender(const GURL& url) {
    return AddPrerenderFromLinkRelPrerender(-1, -1,
                                            url,
                                            GURL());
  }

  void set_rate_limit_enabled(bool enabled) {
    mutable_config().rate_limit_enabled = enabled;
  }

  PrerenderContents* next_prerender_contents() {
    return next_prerender_contents_.get();
  }

 private:
  void SetNextPrerenderContents(DummyPrerenderContents* prerender_contents) {
    DCHECK(!next_prerender_contents_.get());
    next_prerender_contents_.reset(prerender_contents);
    if (prerender_contents->expected_final_status() == FINAL_STATUS_USED)
      used_prerender_contents_.push_back(prerender_contents);
  }

  virtual base::Time GetCurrentTime() const OVERRIDE {
    return time_;
  }

  virtual base::TimeTicks GetCurrentTimeTicks() const OVERRIDE {
    return time_ticks_;
  }

  virtual PrerenderContents* CreatePrerenderContents(
      const GURL& url,
      const GURL& referrer,
      Origin origin,
      uint8 experiment_id) OVERRIDE {
    DCHECK(next_prerender_contents_.get());
    return next_prerender_contents_.release();
  }

  base::Time time_;
  base::TimeTicks time_ticks_;
  scoped_ptr<PrerenderContents> next_prerender_contents_;
  // PrerenderContents with an |expected_final_status| of FINAL_STATUS_USED,
  // tracked so they will be automatically deleted.
  ScopedVector<PrerenderContents> used_prerender_contents_;

  PrerenderTracker* prerender_tracker_;
};

class RestorePrerenderMode {
 public:
  RestorePrerenderMode() : prev_mode_(PrerenderManager::GetMode()) {
  }

  ~RestorePrerenderMode() { PrerenderManager::SetMode(prev_mode_); }
 private:
  PrerenderManager::PrerenderManagerMode prev_mode_;
};

}  // namespace

class PrerenderManagerTest : public testing::Test {
 public:
  PrerenderManagerTest() : ui_thread_(BrowserThread::UI, &message_loop_),
                           prerender_manager_(
                               new TestPrerenderManager(prerender_tracker())) {
  }

  TestPrerenderManager* prerender_manager() {
    return prerender_manager_.get();
  }

 private:
  PrerenderTracker* prerender_tracker() {
    return g_browser_process->prerender_tracker();
  }

  // Needed to pass PrerenderManager's DCHECKs.
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestPrerenderManager> prerender_manager_;
};

TEST_F(PrerenderManagerTest, EmptyTest) {
  EXPECT_FALSE(prerender_manager()->MaybeUsePrerenderedPage(
      NULL,
      GURL("http://www.google.com/"),
      GURL()));

  EXPECT_FALSE(prerender_manager()->MaybeUsePrerenderedPage(
      NULL,
      GURL("http://www.google.com/search"),
      GURL("http://www.google.com")));
}

TEST_F(PrerenderManagerTest, FoundTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_USED);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->has_started());
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(url));
}

// Make sure that if queue a request, and a second prerender request for the
// same URL comes in, that we drop the second request and keep the first one.
TEST_F(PrerenderManagerTest, DropSecondRequestTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_USED);
  DummyPrerenderContents* null = NULL;
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents->has_started());

  DummyPrerenderContents* prerender_contents1 =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_MANAGER_SHUTDOWN);
  EXPECT_FALSE(prerender_manager()->AddSimplePrerender(url));
  EXPECT_EQ(prerender_contents1,
            prerender_manager()->next_prerender_contents());
  EXPECT_FALSE(prerender_contents1->has_started());

  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(url));
}

// Ensure that we expire a prerendered page after the max. permitted time.
TEST_F(PrerenderManagerTest, ExpireTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_TIMED_OUT);
  DummyPrerenderContents* null = NULL;
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents->has_started());
  prerender_manager()->AdvanceTime(prerender_manager()->config().max_age
                                   + base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
}

// LRU Test.  Make sure that if we prerender more than one request, that
// the oldest one will be dropped.
TEST_F(PrerenderManagerTest, DropOldestRequestTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_EVICTED);
  DummyPrerenderContents* null = NULL;
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents->has_started());

  GURL url1("http://news.google.com/");
  DummyPrerenderContents* prerender_contents1 =
      prerender_manager()->CreateNextPrerenderContents(
          url1,
          FINAL_STATUS_USED);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url1));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents1->has_started());

  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
  ASSERT_EQ(prerender_contents1, prerender_manager()->GetEntry(url1));
}

// Two element prerender test.  Ensure that the LRU operates correctly if we
// permit 2 elements to be kept prerendered.
TEST_F(PrerenderManagerTest, TwoElementPrerenderTest) {
  prerender_manager()->mutable_config().max_elements = 2;
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_EVICTED);
  DummyPrerenderContents* null = NULL;
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents->has_started());

  GURL url1("http://news.google.com/");
  DummyPrerenderContents* prerender_contents1 =
      prerender_manager()->CreateNextPrerenderContents(
          url1,
          FINAL_STATUS_USED);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url1));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents1->has_started());

  GURL url2("http://images.google.com/");
  DummyPrerenderContents* prerender_contents2 =
      prerender_manager()->CreateNextPrerenderContents(
          url2,
          FINAL_STATUS_USED);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url2));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents2->has_started());

  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
  ASSERT_EQ(prerender_contents1, prerender_manager()->GetEntry(url1));
  ASSERT_EQ(prerender_contents2, prerender_manager()->GetEntry(url2));
}

TEST_F(PrerenderManagerTest, AliasURLTest) {
  GURL url("http://www.google.com/");
  GURL alias_url1("http://www.google.com/index.html");
  GURL alias_url2("http://google.com/");
  GURL not_an_alias_url("http://google.com/index.html");
  std::vector<GURL> alias_urls;
  alias_urls.push_back(alias_url1);
  alias_urls.push_back(alias_url2);

  // Test that all of the aliases work, but not_an_alias_url does not.
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));
  ASSERT_EQ(NULL, prerender_manager()->GetEntry(not_an_alias_url));
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(alias_url1));
  prerender_contents = prerender_manager()->CreateNextPrerenderContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(alias_url2));
  prerender_contents = prerender_manager()->CreateNextPrerenderContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(url));

  // Test that alias URLs can not be added.
  prerender_contents = prerender_manager()->CreateNextPrerenderContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));
  EXPECT_FALSE(prerender_manager()->AddSimplePrerender(url));
  EXPECT_FALSE(prerender_manager()->AddSimplePrerender(alias_url1));
  EXPECT_FALSE(prerender_manager()->AddSimplePrerender(alias_url2));
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(url));
}

// Ensure that we ignore prerender requests within the rate limit.
TEST_F(PrerenderManagerTest, RateLimitInWindowTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_MANAGER_SHUTDOWN);
  DummyPrerenderContents* null = NULL;
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents->has_started());

  prerender_manager()->set_rate_limit_enabled(true);
  prerender_manager()->AdvanceTimeTicks(base::TimeDelta::FromMilliseconds(1));

  GURL url1("http://news.google.com/");
  prerender_manager()->CreateNextPrerenderContents(
      url,
      FINAL_STATUS_MANAGER_SHUTDOWN);
  EXPECT_FALSE(prerender_manager()->AddSimplePrerender(url1));
  prerender_manager()->set_rate_limit_enabled(false);
}

// Ensure that we don't ignore prerender requests outside the rate limit.
TEST_F(PrerenderManagerTest, RateLimitOutsideWindowTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_EVICTED);
  DummyPrerenderContents* null = NULL;
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents->has_started());

  prerender_manager()->set_rate_limit_enabled(true);
  prerender_manager()->AdvanceTimeTicks(
      base::TimeDelta::FromMilliseconds(2000));

  GURL url1("http://news.google.com/");
  DummyPrerenderContents* rate_limit_prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url1,
          FINAL_STATUS_MANAGER_SHUTDOWN);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url1));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(rate_limit_prerender_contents->has_started());
  prerender_manager()->set_rate_limit_enabled(false);
}

TEST_F(PrerenderManagerTest, PendingPrerenderTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_USED);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));

  int child_id;
  int route_id;
  ASSERT_TRUE(prerender_contents->GetChildId(&child_id));
  ASSERT_TRUE(prerender_contents->GetRouteId(&route_id));

  GURL pending_url("http://news.google.com/");

  EXPECT_TRUE(prerender_manager()->AddPrerenderFromLinkRelPrerender(
      child_id, route_id,
      pending_url, url));

  EXPECT_TRUE(prerender_manager()->IsPendingEntry(pending_url));
  EXPECT_TRUE(prerender_contents->has_started());
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(url));
}

// Tests that a PrerenderManager created for a browser session in the control
// group will not be able to override FINAL_STATUS_CONTROL_GROUP.
TEST_F(PrerenderManagerTest, ControlGroup) {
  RestorePrerenderMode restore_prerender_mode;
  PrerenderManager::SetMode(
      PrerenderManager::PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP);
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_CONTROL_GROUP);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));
  EXPECT_FALSE(prerender_contents->has_started());
}

// Tests that prerendering is cancelled when the source render view does not
// exist.  On failure, the DCHECK in CreatePrerenderContents() above should be
// triggered.
TEST_F(PrerenderManagerTest, SourceRenderViewClosed) {
  GURL url("http://www.google.com/");
  prerender_manager()->CreateNextPrerenderContents(
      url,
      FINAL_STATUS_MANAGER_SHUTDOWN);
  EXPECT_FALSE(prerender_manager()->AddPrerenderFromLinkRelPrerender(
      100, 100, url, GURL()));
}

// Tests that the prerender manager ignores fragment references when matching
// prerender URLs in the case the fragment is not in the prerender URL.
TEST_F(PrerenderManagerTest, PageMatchesFragmentTest) {
  GURL url("http://www.google.com/");
  GURL fragment_url("http://www.google.com/#test");

  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(url,
                                                       FINAL_STATUS_USED);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->has_started());
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(fragment_url));
}

// Tests that the prerender manager ignores fragment references when matching
// prerender URLs in the case the fragment is in the prerender URL.
TEST_F(PrerenderManagerTest, FragmentMatchesPageTest) {
  GURL url("http://www.google.com/");
  GURL fragment_url("http://www.google.com/#test");

  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(fragment_url,
                                                       FINAL_STATUS_USED);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(fragment_url));
  EXPECT_TRUE(prerender_contents->has_started());
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(url));
}

// Tests that the prerender manager ignores fragment references when matching
// prerender URLs in the case the fragment is in both URLs.
TEST_F(PrerenderManagerTest, FragmentMatchesFragmentTest) {
  GURL fragment_url("http://www.google.com/#test");
  GURL other_fragment_url("http://www.google.com/#other_test");

  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(fragment_url,
                                                       FINAL_STATUS_USED);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(fragment_url));
  EXPECT_TRUE(prerender_contents->has_started());
  ASSERT_EQ(prerender_contents,
            prerender_manager()->GetEntry(other_fragment_url));
}

// Make sure that clearing works as expected.
TEST_F(PrerenderManagerTest, ClearTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_CACHE_OR_HISTORY_CLEARED);
  EXPECT_TRUE(prerender_manager()->AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->has_started());
  prerender_manager()->ClearData(PrerenderManager::CLEAR_PRERENDER_CONTENTS);
  DummyPrerenderContents* null = NULL;
  EXPECT_EQ(null, prerender_manager()->GetEntry(url));
}

}  // namespace prerender
