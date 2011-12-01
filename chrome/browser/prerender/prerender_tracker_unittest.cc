// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace prerender {

namespace {

class TestPrerenderManager : public PrerenderManager {
 public:
  explicit TestPrerenderManager(PrerenderTracker* prerender_tracker) :
      PrerenderManager(NULL, prerender_tracker) {
    mutable_config().rate_limit_enabled = false;
  }

  virtual void DestroyPrerenderForRenderView(
      int process_id, int view_id, FinalStatus final_status) OVERRIDE {
    cancelled_id_pairs_.insert(std::make_pair(process_id, view_id));
  }

  bool WasPrerenderCancelled(int child_id, int route_id) {
    std::pair<int, int> child_route_id_pair(child_id, route_id);
    return cancelled_id_pairs_.count(child_route_id_pair) != 0;
  }

  // Set of all the RenderViews that have been cancelled.
  std::set<std::pair<int, int> > cancelled_id_pairs_;
};

}  // namespace

class PrerenderTrackerTest : public testing::Test {
 public:
  PrerenderTrackerTest() :
      ui_thread_(BrowserThread::UI, &message_loop_),
      io_thread_(BrowserThread::IO, &message_loop_),
      prerender_manager_(new TestPrerenderManager(prerender_tracker())) {
  }

  TestPrerenderManager* prerender_manager() {
    return prerender_manager_.get();
  }

  PrerenderTracker* prerender_tracker() {
    return g_browser_process->prerender_tracker();
  }

  int GetCurrentStatus(int child_id, int route_id) {
    FinalStatus final_status;
    if (!prerender_tracker()->GetFinalStatus(child_id, route_id,
                                             &final_status)) {
      return -1;
    }
    return final_status;
  }

  // Runs any tasks queued on either thread.
  void RunEvents() {
    message_loop_.RunAllPending();
  }

 private:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;

  scoped_ptr<TestPrerenderManager> prerender_manager_;
};

// Check that a non-existant RenderView is handled correctly.
TEST_F(PrerenderTrackerTest, PrerenderTrackerNull) {
  FinalStatus final_status;
  EXPECT_FALSE(prerender_tracker()->TryUse(0, 0));
  EXPECT_FALSE(prerender_tracker()->TryCancel(0, 0, FINAL_STATUS_HTTPS));
  EXPECT_FALSE(prerender_tracker()->TryCancelOnIOThread(
      0, 0, FINAL_STATUS_HTTPS));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(0, 0));
  EXPECT_FALSE(prerender_tracker()->GetFinalStatus(0, 0, &final_status));
  EXPECT_FALSE(prerender_manager()->WasPrerenderCancelled(0, 0));
}

// Check that a page that is used is handled correctly.
TEST_F(PrerenderTrackerTest, PrerenderTrackerUsed) {
  prerender_tracker()->OnPrerenderingStarted(0, 0, prerender_manager());
  EXPECT_EQ(FINAL_STATUS_MAX, GetCurrentStatus(0, 0));

  // This calls AddPrerenderOnIOThreadTask().
  RunEvents();

  EXPECT_TRUE(prerender_tracker()->IsPrerenderingOnIOThread(0, 0));
  EXPECT_EQ(FINAL_STATUS_MAX, GetCurrentStatus(0, 0));

  // Display the prerendered RenderView.
  EXPECT_TRUE(prerender_tracker()->TryUse(0, 0));

  // Make sure the page can't be destroyed or claim it was destroyed after
  // it's been used.
  EXPECT_FALSE(prerender_tracker()->TryCancel(0, 0, FINAL_STATUS_HTTPS));
  EXPECT_FALSE(prerender_tracker()->TryCancelOnIOThread(
      0, 0, FINAL_STATUS_TIMED_OUT));
  EXPECT_EQ(FINAL_STATUS_USED, GetCurrentStatus(0, 0));

  // This would call DestroyPrerenderForChildRouteIdPair(), if the prerender
  // were cancelled.
  RunEvents();

  // These functions should all behave as before.
  EXPECT_FALSE(prerender_tracker()->TryCancel(0, 0, FINAL_STATUS_HTTPS));
  EXPECT_FALSE(prerender_tracker()->TryCancelOnIOThread(
      0, 0, FINAL_STATUS_TIMED_OUT));
  EXPECT_EQ(FINAL_STATUS_USED, GetCurrentStatus(0, 0));

  // This calls DestroyPrerenderForChildRouteIdPair().
  prerender_tracker()->OnPrerenderingFinished(0, 0);
  EXPECT_TRUE(prerender_tracker()->IsPrerenderingOnIOThread(0, 0));

  // This calls RemovePrerenderOnIOThreadTask().
  RunEvents();

  FinalStatus final_status;
  EXPECT_FALSE(prerender_tracker()->GetFinalStatus(0, 0, &final_status));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(0, 0));
  EXPECT_FALSE(prerender_tracker()->TryCancel(0, 0, FINAL_STATUS_HTTPS));
  EXPECT_FALSE(prerender_manager()->WasPrerenderCancelled(0, 0));
}

// Check that a prerendered page cancelled by TryCancel() is handled correctly.
TEST_F(PrerenderTrackerTest, PrerenderTrackerCancelled) {
  prerender_tracker()->OnPrerenderingStarted(0, 0, prerender_manager());
  EXPECT_EQ(FINAL_STATUS_MAX, GetCurrentStatus(0, 0));

  // This calls AddPrerenderOnIOThreadTask().
  RunEvents();

  // Cancel the prerender.
  EXPECT_TRUE(prerender_tracker()->TryCancel(0, 0, FINAL_STATUS_HTTPS));

  EXPECT_FALSE(prerender_tracker()->TryUse(0, 0));
  EXPECT_TRUE(prerender_tracker()->TryCancel(0, 0, FINAL_STATUS_TIMED_OUT));
  EXPECT_TRUE(prerender_tracker()->TryCancelOnIOThread(
      0, 0, FINAL_STATUS_TIMED_OUT));
  EXPECT_EQ(FINAL_STATUS_HTTPS, GetCurrentStatus(0, 0));

  // This calls DestroyPrerenderForChildRouteIdPair().
  RunEvents();
  EXPECT_TRUE(prerender_manager()->WasPrerenderCancelled(0, 0));

  // These should all work until the prerendering RenderViewHost is destroyed.
  EXPECT_TRUE(prerender_tracker()->TryCancel(0, 0, FINAL_STATUS_TIMED_OUT));
  EXPECT_TRUE(prerender_tracker()->TryCancelOnIOThread(
      0, 0, FINAL_STATUS_TIMED_OUT));
  EXPECT_EQ(FINAL_STATUS_HTTPS, GetCurrentStatus(0, 0));

  prerender_tracker()->OnPrerenderingFinished(0, 0);
  EXPECT_TRUE(prerender_tracker()->IsPrerenderingOnIOThread(0, 0));

  // This calls RemovePrerenderOnIOThreadTask().
  RunEvents();

  FinalStatus final_status;
  EXPECT_FALSE(prerender_tracker()->GetFinalStatus(0, 0, &final_status));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(0, 0));
}

// Check that a prerendered page cancelled on the IO thread by
// TryCancelOnIOThread() is handled correctly.
TEST_F(PrerenderTrackerTest, PrerenderTrackerCancelledOnIO) {
  prerender_tracker()->OnPrerenderingStarted(0, 0, prerender_manager());
  EXPECT_EQ(FINAL_STATUS_MAX, GetCurrentStatus(0, 0));

  // This calls AddPrerenderOnIOThreadTask().
  RunEvents();

  // Cancel the prerender.
  EXPECT_TRUE(prerender_tracker()->TryCancelOnIOThread(
      0, 0, FINAL_STATUS_TIMED_OUT));

  EXPECT_FALSE(prerender_tracker()->TryUse(0, 0));
  EXPECT_TRUE(prerender_tracker()->TryCancel(0, 0, FINAL_STATUS_HTTPS));
  EXPECT_TRUE(prerender_tracker()->TryCancelOnIOThread(
      0, 0, FINAL_STATUS_HTTPS));
  EXPECT_EQ(FINAL_STATUS_TIMED_OUT, GetCurrentStatus(0, 0));

  // This calls DestroyPrerenderForChildRouteIdPair().
  RunEvents();
  EXPECT_TRUE(prerender_manager()->WasPrerenderCancelled(0, 0));

  // These should all work until the prerendering RenderViewHost is destroyed.
  EXPECT_TRUE(prerender_tracker()->TryCancel(0, 0, FINAL_STATUS_HTTPS));
  EXPECT_TRUE(prerender_tracker()->TryCancelOnIOThread(
      0, 0, FINAL_STATUS_HTTPS));
  EXPECT_EQ(FINAL_STATUS_TIMED_OUT, GetCurrentStatus(0, 0));

  prerender_tracker()->OnPrerenderingFinished(0, 0);
  EXPECT_TRUE(prerender_tracker()->IsPrerenderingOnIOThread(0, 0));

  // This calls RemovePrerenderOnIOThreadTask().
  RunEvents();

  FinalStatus final_status;
  EXPECT_FALSE(prerender_tracker()->GetFinalStatus(0, 0, &final_status));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(0, 0));
}

// Check that a prerendered page cancelled before it reaches the IO thread is
// handled correctly.
TEST_F(PrerenderTrackerTest, PrerenderTrackerCancelledFast) {
  prerender_tracker()->OnPrerenderingStarted(0, 0, prerender_manager());
  // Cancel the prerender.
  EXPECT_TRUE(prerender_tracker()->TryCancel(0, 0, FINAL_STATUS_HTTPS));

  EXPECT_FALSE(prerender_tracker()->TryUse(0, 0));
  EXPECT_TRUE(prerender_tracker()->TryCancel(0, 0, FINAL_STATUS_TIMED_OUT));

  // This calls AddPrerenderOnIOThreadTask() and
  // DestroyPrerenderForChildRouteIdPair().
  RunEvents();
  EXPECT_TRUE(prerender_manager()->WasPrerenderCancelled(0, 0));

  EXPECT_TRUE(prerender_tracker()->TryCancelOnIOThread(
      0, 0, FINAL_STATUS_TIMED_OUT));
  EXPECT_TRUE(prerender_tracker()->TryCancel(0, 0, FINAL_STATUS_TIMED_OUT));
  EXPECT_EQ(FINAL_STATUS_HTTPS, GetCurrentStatus(0, 0));

  prerender_tracker()->OnPrerenderingFinished(0, 0);

  // This calls RemovePrerenderOnIOThreadTask().
  RunEvents();

  FinalStatus final_status;
  EXPECT_FALSE(prerender_tracker()->GetFinalStatus(0, 0, &final_status));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(0, 0));
}

// Check that handling two pages at once works.
TEST_F(PrerenderTrackerTest, PrerenderTrackerMultiple) {
  prerender_tracker()->OnPrerenderingStarted(0, 0, prerender_manager());

  // This calls AddPrerenderOnIOThreadTask().
  RunEvents();
  EXPECT_TRUE(prerender_tracker()->IsPrerenderingOnIOThread(0, 0));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(1, 2));
  EXPECT_FALSE(prerender_tracker()->TryUse(1, 2));
  EXPECT_FALSE(prerender_tracker()->TryCancel(1, 2, FINAL_STATUS_HTTPS));

  // Start second prerender.
  prerender_tracker()->OnPrerenderingStarted(1, 2, prerender_manager());
  // This calls AddPrerenderOnIOThreadTask().
  RunEvents();

  // Use (0, 0).
  EXPECT_TRUE(prerender_tracker()->TryUse(0, 0));
  EXPECT_EQ(FINAL_STATUS_USED, GetCurrentStatus(0, 0));
  EXPECT_EQ(FINAL_STATUS_MAX, GetCurrentStatus(1, 2));

  // Cancel (1, 2).
  EXPECT_TRUE(prerender_tracker()->TryCancelOnIOThread(
      1, 2, FINAL_STATUS_HTTPS));

  EXPECT_FALSE(prerender_tracker()->TryCancel(0, 0, FINAL_STATUS_HTTPS));
  EXPECT_EQ(FINAL_STATUS_USED, GetCurrentStatus(0, 0));

  EXPECT_FALSE(prerender_tracker()->TryUse(1, 2));
  EXPECT_TRUE(prerender_tracker()->TryCancel(1, 2, FINAL_STATUS_HTTPS));
  EXPECT_EQ(FINAL_STATUS_HTTPS, GetCurrentStatus(1, 2));

  // This calls DestroyPrerenderForChildRouteIdPair().
  RunEvents();
  EXPECT_FALSE(prerender_manager()->WasPrerenderCancelled(0, 0));
  EXPECT_TRUE(prerender_manager()->WasPrerenderCancelled(1, 2));

  prerender_tracker()->OnPrerenderingFinished(0, 0);
  prerender_tracker()->OnPrerenderingFinished(1, 2);

  // This calls RemovePrerenderOnIOThreadTask().
  RunEvents();

  FinalStatus final_status;
  EXPECT_FALSE(prerender_tracker()->GetFinalStatus(0, 0, &final_status));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(0, 0));

  EXPECT_FALSE(prerender_tracker()->GetFinalStatus(1, 2, &final_status));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(1, 2));
}

// Tests that Prerender tracking works correctly
TEST_F(PrerenderTrackerTest, URLCounter) {
  URLCounter url_counter;
  GURL example_url("http://www.example.com");
  GURL gmail_url("https://www.gmail.com");

  EXPECT_FALSE(url_counter.MatchesURL(example_url));
  EXPECT_FALSE(url_counter.MatchesURL(gmail_url));

  url_counter.AddURL(example_url);
  url_counter.AddURL(example_url);
  url_counter.AddURL(gmail_url);
  EXPECT_TRUE(url_counter.MatchesURL(example_url));
  EXPECT_TRUE(url_counter.MatchesURL(gmail_url));

  std::vector<GURL> remove_urls;
  remove_urls.push_back(example_url);
  remove_urls.push_back(gmail_url);
  url_counter.RemoveURLs(remove_urls);
  EXPECT_TRUE(url_counter.MatchesURL(example_url));
  EXPECT_FALSE(url_counter.MatchesURL(gmail_url));

  remove_urls.clear();
  remove_urls.push_back(example_url);
  url_counter.RemoveURLs(remove_urls);
  EXPECT_FALSE(url_counter.MatchesURL(example_url));
  EXPECT_FALSE(url_counter.MatchesURL(gmail_url));

  url_counter.AddURL(example_url);
  url_counter.AddURL(example_url);
  EXPECT_TRUE(url_counter.MatchesURL(example_url));
  EXPECT_FALSE(url_counter.MatchesURL(gmail_url));

  remove_urls.clear();
  remove_urls.push_back(example_url);
  remove_urls.push_back(example_url);
  url_counter.RemoveURLs(remove_urls);
  EXPECT_FALSE(url_counter.MatchesURL(example_url));
  EXPECT_FALSE(url_counter.MatchesURL(gmail_url));
}

}  // namespace prerender
