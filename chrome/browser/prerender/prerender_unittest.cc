// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_link_manager.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_view_host.h"
#include "content/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"

using content::BrowserThread;

namespace prerender {

namespace {

class DummyPrerenderContents : public PrerenderContents {
 public:
  DummyPrerenderContents(PrerenderManager* prerender_manager,
                         PrerenderTracker* prerender_tracker,
                         const GURL& url,
                         Origin origin,
                         FinalStatus expected_final_status)
      : PrerenderContents(prerender_manager, prerender_tracker,
                          NULL, url, content::Referrer(),
                          origin, PrerenderManager::kNoExperiment),
        has_started_(false),
        expected_final_status_(expected_final_status) {
  }

  virtual ~DummyPrerenderContents() {
    EXPECT_EQ(expected_final_status_, final_status());
  }

  virtual void StartPrerendering(
      int ALLOW_UNUSED creator_child_id,
      const gfx::Size& ALLOW_UNUSED size,
      content::SessionStorageNamespace* ALLOW_UNUSED
          session_storage_namespace) OVERRIDE {
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

  bool prerendering_has_been_cancelled() const {
    return PrerenderContents::prerendering_has_been_cancelled();
  }

 private:
  bool has_started_;
  FinalStatus expected_final_status_;
};

class TestPrerenderManager : public PrerenderManager {
 public:
  explicit TestPrerenderManager(PrerenderTracker* prerender_tracker)
      : PrerenderManager(&profile_, prerender_tracker),
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
                                   ORIGIN_LINK_REL_PRERENDER,
                                   expected_final_status);
    SetNextPrerenderContents(prerender_contents);
    return prerender_contents;
  }

  DummyPrerenderContents* CreateNextPrerenderContents(
      const GURL& url,
      Origin origin,
      FinalStatus expected_final_status) {
    DummyPrerenderContents* prerender_contents =
        new DummyPrerenderContents(this, prerender_tracker_, url,
                                   origin, expected_final_status);
    SetNextPrerenderContents(prerender_contents);
    return prerender_contents;
  }

  DummyPrerenderContents* CreateNextPrerenderContents(
      const GURL& url,
      const std::vector<GURL>& alias_urls,
      FinalStatus expected_final_status) {
    DummyPrerenderContents* prerender_contents =
        new DummyPrerenderContents(this, prerender_tracker_, url,
                                   ORIGIN_LINK_REL_PRERENDER,
                                   expected_final_status);
    for (std::vector<GURL>::const_iterator it = alias_urls.begin();
         it != alias_urls.end();
         ++it) {
      EXPECT_TRUE(prerender_contents->AddAliasURL(*it));
    }
    SetNextPrerenderContents(prerender_contents);
    return prerender_contents;
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
      const content::Referrer& referrer,
      Origin origin,
      uint8 experiment_id) OVERRIDE {
    DCHECK(next_prerender_contents_.get());
    DCHECK_EQ(next_prerender_contents_->prerender_url(), url);
    DCHECK_EQ(next_prerender_contents_->origin(), origin);
    return next_prerender_contents_.release();
  }

  base::Time time_;
  base::TimeTicks time_ticks_;
  scoped_ptr<PrerenderContents> next_prerender_contents_;
  // PrerenderContents with an |expected_final_status| of FINAL_STATUS_USED,
  // tracked so they will be automatically deleted.
  ScopedVector<PrerenderContents> used_prerender_contents_;

  PrerenderTracker* prerender_tracker_;

  TestingProfile profile_;
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

class PrerenderTest : public testing::Test {
 public:
  static const int kDefaultChildId = -1;
  static const int kDefaultRenderViewRouteId = -1;

  PrerenderTest() : ui_thread_(BrowserThread::UI, &message_loop_),
                    prerender_manager_(
                        new TestPrerenderManager(prerender_tracker())),
                    prerender_link_manager_(
                        new PrerenderLinkManager(prerender_manager_.get())),
                    last_prerender_id_(0) {
    // Enable omnibox prerendering.
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kPrerenderFromOmnibox,
        switches::kPrerenderFromOmniboxSwitchValueEnabled);
  }

  TestPrerenderManager* prerender_manager() {
    return prerender_manager_.get();
  }

  PrerenderLinkManager* prerender_link_manager() {
    return prerender_link_manager_.get();
  }

  bool IsEmptyPrerenderLinkManager() {
    return prerender_link_manager_->IsEmpty();
  }

  int last_prerender_id() const {
    return last_prerender_id_;
  }

  int GetNextPrerenderID() {
    return ++last_prerender_id_;
  }

  // Shorthand to add a simple preload with a reasonable source.
  bool AddSimplePrerender(const GURL& url) {
    return prerender_link_manager()->OnAddPrerender(
        kDefaultChildId, GetNextPrerenderID(),
        url, content::Referrer(),
        gfx::Size(), kDefaultRenderViewRouteId);
  }

 private:
  PrerenderTracker* prerender_tracker() {
    return g_browser_process->prerender_tracker();
  }

  // Needed to pass PrerenderManager's DCHECKs.
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestPrerenderManager> prerender_manager_;
  scoped_ptr<PrerenderLinkManager> prerender_link_manager_;
  int last_prerender_id_;
};

TEST_F(PrerenderTest, EmptyTest) {
  EXPECT_FALSE(prerender_manager()->MaybeUsePrerenderedPage(
      NULL,
      GURL("http://www.google.com/")));
}

TEST_F(PrerenderTest, FoundTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->has_started());
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(url));
}

// Make sure that if queue a request, and a second prerender request for the
// same URL comes in, that we drop the second request and keep the first one.
TEST_F(PrerenderTest, DropSecondRequestTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_USED);
  DummyPrerenderContents* null = NULL;
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents->has_started());

  DummyPrerenderContents* prerender_contents1 =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_MANAGER_SHUTDOWN);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_EQ(prerender_contents1,
            prerender_manager()->next_prerender_contents());
  EXPECT_FALSE(prerender_contents1->has_started());

  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(url));
}

// Ensure that we expire a prerendered page after the max. permitted time.
TEST_F(PrerenderTest, ExpireTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_TIMED_OUT);
  DummyPrerenderContents* null = NULL;
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents->has_started());
  prerender_manager()->AdvanceTime(prerender_manager()->GetMaxAge() +
                                   base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
}

// LRU Test.  Make sure that if we prerender more than one request, that
// the oldest one will be dropped.
TEST_F(PrerenderTest, DropOldestRequestTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_EVICTED);
  DummyPrerenderContents* null = NULL;
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents->has_started());

  GURL url1("http://news.google.com/");
  DummyPrerenderContents* prerender_contents1 =
      prerender_manager()->CreateNextPrerenderContents(
          url1,
          FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url1));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents1->has_started());

  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
  ASSERT_EQ(prerender_contents1, prerender_manager()->GetEntry(url1));
}

// Two element prerender test.  Ensure that the LRU operates correctly if we
// permit 2 elements to be kept prerendered.
TEST_F(PrerenderTest, TwoElementPrerenderTest) {
  prerender_manager()->mutable_config().max_elements = 2;
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_EVICTED);
  DummyPrerenderContents* null = NULL;
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents->has_started());

  GURL url1("http://news.google.com/");
  DummyPrerenderContents* prerender_contents1 =
      prerender_manager()->CreateNextPrerenderContents(
          url1,
          FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url1));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents1->has_started());

  GURL url2("http://images.google.com/");
  DummyPrerenderContents* prerender_contents2 =
      prerender_manager()->CreateNextPrerenderContents(
          url2,
          FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url2));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents2->has_started());

  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
  ASSERT_EQ(prerender_contents1, prerender_manager()->GetEntry(url1));
  ASSERT_EQ(prerender_contents2, prerender_manager()->GetEntry(url2));
}

TEST_F(PrerenderTest, AliasURLTest) {
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
  EXPECT_TRUE(AddSimplePrerender(url));
  ASSERT_EQ(NULL, prerender_manager()->GetEntry(not_an_alias_url));
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(alias_url1));
  prerender_contents = prerender_manager()->CreateNextPrerenderContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(alias_url2));
  prerender_contents = prerender_manager()->CreateNextPrerenderContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(url));

  // Test that alias URLs can not be added.
  prerender_contents = prerender_manager()->CreateNextPrerenderContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(AddSimplePrerender(alias_url1));
  EXPECT_TRUE(AddSimplePrerender(alias_url2));
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(url));
}

// Ensure that we ignore prerender requests within the rate limit.
TEST_F(PrerenderTest, RateLimitInWindowTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_MANAGER_SHUTDOWN);
  DummyPrerenderContents* null = NULL;
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents->has_started());

  prerender_manager()->set_rate_limit_enabled(true);
  prerender_manager()->AdvanceTimeTicks(base::TimeDelta::FromMilliseconds(1));

  GURL url1("http://news.google.com/");
  prerender_manager()->CreateNextPrerenderContents(
      url,
      FINAL_STATUS_MANAGER_SHUTDOWN);
  EXPECT_FALSE(AddSimplePrerender(url1));
  prerender_manager()->set_rate_limit_enabled(false);
}

// Ensure that we don't ignore prerender requests outside the rate limit.
TEST_F(PrerenderTest, RateLimitOutsideWindowTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_EVICTED);
  DummyPrerenderContents* null = NULL;
  EXPECT_TRUE(AddSimplePrerender(url));
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
  EXPECT_TRUE(AddSimplePrerender(url1));
  EXPECT_EQ(null, prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(rate_limit_prerender_contents->has_started());
  prerender_manager()->set_rate_limit_enabled(false);
}

TEST_F(PrerenderTest, PendingPrerenderTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));

  int child_id;
  int route_id;
  ASSERT_TRUE(prerender_contents->GetChildId(&child_id));
  ASSERT_TRUE(prerender_contents->GetRouteId(&route_id));

  GURL pending_url("http://news.google.com/");

  EXPECT_TRUE(prerender_manager()->AddPrerenderFromLinkRelPrerender(
      child_id, route_id,
      pending_url, content::Referrer(url, WebKit::WebReferrerPolicyDefault),
      gfx::Size()));

  EXPECT_TRUE(prerender_manager()->IsPendingEntry(pending_url));
  EXPECT_TRUE(prerender_contents->has_started());
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(url));
}

// Tests that a PrerenderManager created for a browser session in the control
// group works as expected.
TEST_F(PrerenderTest, ControlGroup) {
  RestorePrerenderMode restore_prerender_mode;
  PrerenderManager::SetMode(
      PrerenderManager::PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP);
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_MANAGER_SHUTDOWN);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_FALSE(prerender_contents->has_started());
}

// Tests that prerendering is cancelled when the source render view does not
// exist.  On failure, the DCHECK in CreatePrerenderContents() above should be
// triggered.
TEST_F(PrerenderTest, SourceRenderViewClosed) {
  GURL url("http://www.google.com/");
  prerender_manager()->CreateNextPrerenderContents(
      url,
      FINAL_STATUS_MANAGER_SHUTDOWN);
  EXPECT_FALSE(prerender_link_manager()->OnAddPrerender(
      100, GetNextPrerenderID(), url,
      content::Referrer(), gfx::Size(), 200));
}

// Tests that the prerender manager ignores fragment references when matching
// prerender URLs in the case the fragment is not in the prerender URL.
TEST_F(PrerenderTest, PageMatchesFragmentTest) {
  GURL url("http://www.google.com/");
  GURL fragment_url("http://www.google.com/#test");

  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(url,
                                                       FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->has_started());
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(fragment_url));
}

// Tests that the prerender manager ignores fragment references when matching
// prerender URLs in the case the fragment is in the prerender URL.
// TODO(gavinp): Re-enable this in the Prerender API once we're officially
// fragment clean.
TEST_F(PrerenderTest, DISABLED_FragmentMatchesPageTest) {
  GURL url("http://www.google.com/");
  GURL fragment_url("http://www.google.com/#test");

  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(fragment_url,
                                                       FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(fragment_url));
  EXPECT_TRUE(prerender_contents->has_started());
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(url));
}

// Tests that the prerender manager ignores fragment references when matching
// prerender URLs in the case the fragment is in both URLs.
// TODO(gavinp): Re-enable this in the Prerender API once we're officially
// fragment clean.
TEST_F(PrerenderTest, DISABLED_FragmentMatchesFragmentTest) {
  GURL fragment_url("http://www.google.com/#test");
  GURL other_fragment_url("http://www.google.com/#other_test");

  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(fragment_url,
                                                       FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(fragment_url));
  EXPECT_TRUE(prerender_contents->has_started());
  ASSERT_EQ(prerender_contents,
            prerender_manager()->GetEntry(other_fragment_url));
}

// Make sure that clearing works as expected.
TEST_F(PrerenderTest, ClearTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_CACHE_OR_HISTORY_CLEARED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->has_started());
  prerender_manager()->ClearData(PrerenderManager::CLEAR_PRERENDER_CONTENTS);
  DummyPrerenderContents* null = NULL;
  EXPECT_EQ(null, prerender_manager()->FindEntry(url));
}

// Make sure canceling works as expected.
TEST_F(PrerenderTest, CancelAllTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->has_started());
  prerender_manager()->CancelAllPrerenders();
  const DummyPrerenderContents* null = NULL;
  EXPECT_EQ(null, prerender_manager()->FindEntry(url));
}

// Make sure canceling for omnibox works as expected.
TEST_F(PrerenderTest, CancelOmniboxRemovesOmniboxTest) {
  // Check canceling removes the Omnibox url.
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, ORIGIN_OMNIBOX, FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(prerender_manager()->AddPrerenderFromOmnibox(url, NULL));
  EXPECT_TRUE(prerender_contents->has_started());
  prerender_manager()->CancelOmniboxPrerenders();
  const DummyPrerenderContents* null = NULL;
  EXPECT_EQ(null, prerender_manager()->FindEntry(url));
}

TEST_F(PrerenderTest, CancelOmniboxDoesNotRemoveLinkTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, ORIGIN_LINK_REL_PRERENDER, FINAL_STATUS_MANAGER_SHUTDOWN);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->has_started());
  prerender_manager()->CancelOmniboxPrerenders();
  const DummyPrerenderContents* null = NULL;
  EXPECT_NE(null, prerender_manager()->FindEntry(url));
}

TEST_F(PrerenderTest, OmniboxNotAllowedWhenDisabled) {
  prerender_manager()->set_enabled(false);
  EXPECT_FALSE(prerender_manager()->AddPrerenderFromOmnibox(
      GURL("http://www.example.com"), NULL));
}

TEST_F(PrerenderTest, LinkRelNotAllowedWhenDisabled) {
  prerender_manager()->set_enabled(false);
  EXPECT_FALSE(AddSimplePrerender(
      GURL("http://www.example.com")));
}

TEST_F(PrerenderTest, LinkManagerCancel) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimplePrerender(url));

  EXPECT_TRUE(prerender_contents->has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  EXPECT_FALSE(IsEmptyPrerenderLinkManager());
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              last_prerender_id());

  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  DummyPrerenderContents* null = NULL;
  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

TEST_F(PrerenderTest, LinkManagerCancelThenAbandon) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimplePrerender(url));

  EXPECT_TRUE(prerender_contents->has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  EXPECT_FALSE(IsEmptyPrerenderLinkManager());
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              last_prerender_id());

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               last_prerender_id());

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  DummyPrerenderContents* null = NULL;
  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
}

// TODO(gavinp): Update this test after abandon has an effect on Prerenders,
// like shortening the timeouts.
TEST_F(PrerenderTest, LinkManagerAbandon) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_USED);

  EXPECT_TRUE(AddSimplePrerender(url));

  EXPECT_TRUE(prerender_contents->has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  EXPECT_FALSE(IsEmptyPrerenderLinkManager());
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               last_prerender_id());

  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(url));
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

TEST_F(PrerenderTest, LinkManagerCancelTwice) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              last_prerender_id());

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  DummyPrerenderContents* null = NULL;
  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              last_prerender_id());
}

TEST_F(PrerenderTest, LinkManagerAddTwiceCancelTwice) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimplePrerender(url));

  const int first_prerender_id = last_prerender_id();
  EXPECT_TRUE(prerender_contents->has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  EXPECT_TRUE(AddSimplePrerender(url));

  const int second_prerender_id = last_prerender_id();
  EXPECT_TRUE(prerender_contents->has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              first_prerender_id);

  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              second_prerender_id);

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  DummyPrerenderContents* null = NULL;
  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
}

TEST_F(PrerenderTest, LinkManagerAddTwiceCancelTwiceThenAbandonTwice) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimplePrerender(url));

  const int first_prerender_id = last_prerender_id();
  EXPECT_TRUE(prerender_contents->has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  EXPECT_TRUE(AddSimplePrerender(url));

  const int second_prerender_id = last_prerender_id();
  EXPECT_TRUE(prerender_contents->has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              first_prerender_id);

  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              second_prerender_id);

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               first_prerender_id);

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               second_prerender_id);

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  DummyPrerenderContents* null = NULL;
  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
}

// TODO(gavinp): Update this test after abandon has an effect on Prerenders,
// like shortening the timeouts.
TEST_F(PrerenderTest, LinkManagerAddTwiceAbandonTwice) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_USED);

  EXPECT_TRUE(AddSimplePrerender(url));

  const int first_prerender_id = last_prerender_id();
  EXPECT_TRUE(prerender_contents->has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  EXPECT_TRUE(AddSimplePrerender(url));

  const int second_prerender_id = last_prerender_id();
  EXPECT_TRUE(prerender_contents->has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               first_prerender_id);

  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               second_prerender_id);

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->GetEntry(url));
}

// TODO(gavinp): After abandon shortens the expire time on a Prerender,
// add a series of tests testing advancing the time by either the abandon
// or normal expire, and verifying the expected behaviour with groups
// of links.
TEST_F(PrerenderTest, LinkManagerExpireThenCancel) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_TIMED_OUT);

  EXPECT_TRUE(AddSimplePrerender(url));

  EXPECT_TRUE(prerender_contents->has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_manager()->AdvanceTime(prerender_manager()->GetMaxAge() +
                                   base::TimeDelta::FromSeconds(1));

  EXPECT_FALSE(IsEmptyPrerenderLinkManager());
  DummyPrerenderContents* null = NULL;
  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              last_prerender_id());

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
}

TEST_F(PrerenderTest, LinkManagerExpireThenAddAgain) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* first_prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(first_prerender_contents->has_started());
  EXPECT_FALSE(first_prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(first_prerender_contents, prerender_manager()->FindEntry(url));
  prerender_manager()->AdvanceTime(prerender_manager()->GetMaxAge() +
                                   base::TimeDelta::FromSeconds(1));
  DummyPrerenderContents* null = NULL;
  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
  DummyPrerenderContents* second_prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(second_prerender_contents->has_started());
  ASSERT_EQ(second_prerender_contents, prerender_manager()->GetEntry(url));
  // The PrerenderLinkManager is not empty since we never removed the first
  // prerender.
  EXPECT_FALSE(IsEmptyPrerenderLinkManager());
}

TEST_F(PrerenderTest, LinkManagerCancelThenAddAgain) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* first_prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(first_prerender_contents->has_started());
  EXPECT_FALSE(first_prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(first_prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              last_prerender_id());
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(first_prerender_contents->prerendering_has_been_cancelled());
  DummyPrerenderContents* null = NULL;
  ASSERT_EQ(null, prerender_manager()->GetEntry(url));
  DummyPrerenderContents* second_prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(second_prerender_contents->has_started());
  ASSERT_EQ(second_prerender_contents, prerender_manager()->GetEntry(url));
  EXPECT_FALSE(IsEmptyPrerenderLinkManager());
}

}  // namespace prerender

