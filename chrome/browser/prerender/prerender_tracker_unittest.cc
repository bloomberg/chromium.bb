// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <utility>

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace prerender {

namespace {

class TestPrerenderContents : public PrerenderContents {
 public:
  TestPrerenderContents(PrerenderManager* prerender_manager,
                        int child_id, int route_id)
      : PrerenderContents(prerender_manager, static_cast<Profile*>(NULL),
                          GURL(), content::Referrer(), ORIGIN_NONE,
                          PrerenderManager::kNoExperiment),
        child_id_(child_id),
        route_id_(route_id) {
  }

  virtual ~TestPrerenderContents() {
    if (final_status() == FINAL_STATUS_MAX)
      SetFinalStatus(FINAL_STATUS_USED);
  }

  virtual bool GetChildId(int* child_id) const OVERRIDE {
    *child_id = child_id_;
    return true;
  }

  virtual bool GetRouteId(int* route_id) const OVERRIDE {
    *route_id = route_id_;
    return true;
  }

  void Start() {
    AddObserver(prerender_manager()->prerender_tracker());
    prerendering_has_started_ = true;
    NotifyPrerenderStart();
  }

  void Cancel() {
    Destroy(FINAL_STATUS_CANCELLED);
  }

  void Use() {
    SetFinalStatus(FINAL_STATUS_USED);
    PrepareForUse();
  }

 private:
  int child_id_;
  int route_id_;
};

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

  // We never allocate our PrerenderContents in PrerenderManager, so we don't
  // ever want the default pending delete behaviour.
  virtual void MoveEntryToPendingDelete(PrerenderContents* entry,
                                        FinalStatus final_status) OVERRIDE {
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
  static const int kDefaultChildId = 0;
  static const int kDefaultRouteId = 100;

  PrerenderTrackerTest() :
      ui_thread_(BrowserThread::UI, &message_loop_),
      io_thread_(BrowserThread::IO, &message_loop_),
      prerender_manager_(prerender_tracker()),
      test_contents_(&prerender_manager_, kDefaultChildId, kDefaultRouteId) {
  }

  PrerenderTracker* prerender_tracker() {
    return g_browser_process->prerender_tracker();
  }

  TestPrerenderManager* prerender_manager() {
    return &prerender_manager_;
  }

  TestPrerenderContents* test_contents() {
    return &test_contents_;
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
    message_loop_.RunUntilIdle();
  }

 private:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;

  TestPrerenderManager prerender_manager_;
  TestPrerenderContents test_contents_;
};

// Check that a non-existant RenderView is handled correctly.
TEST_F(PrerenderTrackerTest, PrerenderTrackerNull) {
  EXPECT_FALSE(prerender_tracker()->TryUse(kDefaultChildId, kDefaultRouteId));
  EXPECT_FALSE(prerender_tracker()->TryCancel(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_CANCELLED));
  EXPECT_FALSE(prerender_tracker()->TryCancelOnIOThread(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_CANCELLED));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(
      kDefaultChildId, kDefaultRouteId));
  FinalStatus final_status;
  EXPECT_FALSE(prerender_tracker()->GetFinalStatus(
      kDefaultChildId, kDefaultRouteId, &final_status));
  EXPECT_FALSE(prerender_manager()->WasPrerenderCancelled(
      kDefaultChildId, kDefaultRouteId));
}

// Check that a page that is used is handled correctly.
TEST_F(PrerenderTrackerTest, PrerenderTrackerUsed) {
  test_contents()->Start();

  EXPECT_EQ(FINAL_STATUS_MAX,
            GetCurrentStatus(kDefaultChildId, kDefaultRouteId));

  // This calls AddPrerenderOnIOThreadTask().
  RunEvents();

  EXPECT_TRUE(prerender_tracker()->IsPrerenderingOnIOThread(
      kDefaultChildId, kDefaultRouteId));
  EXPECT_EQ(FINAL_STATUS_MAX,
            GetCurrentStatus(kDefaultChildId, kDefaultRouteId));

  // Display the prerendered RenderView.
  EXPECT_TRUE(prerender_tracker()->TryUse(kDefaultChildId, kDefaultRouteId));

  // Make sure the page can't be destroyed or claim it was destroyed after
  // it's been used.
  EXPECT_FALSE(prerender_tracker()->TryCancel(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_CANCELLED));
  EXPECT_FALSE(prerender_tracker()->TryCancelOnIOThread(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_TIMED_OUT));
  EXPECT_EQ(FINAL_STATUS_USED,
            GetCurrentStatus(kDefaultChildId, kDefaultRouteId));

  // This would call DestroyPrerenderForChildRouteIdPair(), if the prerender
  // were cancelled.
  RunEvents();

  // These functions should all behave as before.
  EXPECT_FALSE(prerender_tracker()->TryCancel(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_CANCELLED));
  EXPECT_FALSE(prerender_tracker()->TryCancelOnIOThread(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_TIMED_OUT));
  EXPECT_EQ(FINAL_STATUS_USED, GetCurrentStatus(
      kDefaultChildId, kDefaultRouteId));

  // This calls DestroyPrerenderForChildRouteIdPair().
  test_contents()->Use();
  EXPECT_TRUE(prerender_tracker()->IsPrerenderingOnIOThread(
      kDefaultChildId, kDefaultRouteId));

  // This calls RemovePrerenderOnIOThreadTask().
  RunEvents();

  FinalStatus final_status;
  EXPECT_FALSE(prerender_tracker()->GetFinalStatus(
      kDefaultChildId, kDefaultRouteId, &final_status));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(
      kDefaultChildId, kDefaultRouteId));
  EXPECT_FALSE(prerender_tracker()->TryCancel(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_CANCELLED));
  EXPECT_FALSE(prerender_manager()->WasPrerenderCancelled(
      kDefaultChildId, kDefaultRouteId));
}

// Check that a prerendered page cancelled by TryCancel() is handled correctly.
TEST_F(PrerenderTrackerTest, PrerenderTrackerCancelled) {
  test_contents()->Start();
  EXPECT_EQ(FINAL_STATUS_MAX,
            GetCurrentStatus(kDefaultChildId, kDefaultRouteId));

  // This calls AddPrerenderOnIOThreadTask().
  RunEvents();

  // Cancel the prerender.
  EXPECT_TRUE(prerender_tracker()->TryCancel(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_CANCELLED));

  EXPECT_FALSE(prerender_tracker()->TryUse(kDefaultChildId, kDefaultRouteId));
  EXPECT_TRUE(prerender_tracker()->TryCancel(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_TIMED_OUT));
  EXPECT_TRUE(prerender_tracker()->TryCancelOnIOThread(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_TIMED_OUT));
  EXPECT_EQ(FINAL_STATUS_CANCELLED,
            GetCurrentStatus(kDefaultChildId, kDefaultRouteId));

  // This calls DestroyPrerenderForChildRouteIdPair().
  RunEvents();
  EXPECT_TRUE(prerender_manager()->WasPrerenderCancelled(
      kDefaultChildId, kDefaultRouteId));

  // These should all work until the prerendering RenderViewHost is destroyed.
  EXPECT_TRUE(prerender_tracker()->TryCancel(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_TIMED_OUT));
  EXPECT_TRUE(prerender_tracker()->TryCancelOnIOThread(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_TIMED_OUT));
  EXPECT_EQ(FINAL_STATUS_CANCELLED,
            GetCurrentStatus(kDefaultChildId, kDefaultRouteId));

  test_contents()->Cancel();
  EXPECT_TRUE(prerender_tracker()->IsPrerenderingOnIOThread(
      kDefaultChildId, kDefaultRouteId));

  // This calls RemovePrerenderOnIOThreadTask().
  RunEvents();

  FinalStatus final_status;
  EXPECT_FALSE(prerender_tracker()->GetFinalStatus(
      kDefaultChildId, kDefaultRouteId, &final_status));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(
      kDefaultChildId, kDefaultRouteId));
}

// Check that a prerendered page cancelled on the IO thread by
// TryCancelOnIOThread() is handled correctly.
TEST_F(PrerenderTrackerTest, PrerenderTrackerCancelledOnIO) {
  test_contents()->Start();
  EXPECT_EQ(FINAL_STATUS_MAX,
            GetCurrentStatus(kDefaultChildId, kDefaultRouteId));

  // This calls AddPrerenderOnIOThreadTask().
  RunEvents();

  // Cancel the prerender.
  EXPECT_TRUE(prerender_tracker()->TryCancelOnIOThread(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_TIMED_OUT));

  EXPECT_FALSE(prerender_tracker()->TryUse(kDefaultChildId, kDefaultRouteId));
  EXPECT_TRUE(prerender_tracker()->TryCancel(kDefaultChildId, kDefaultRouteId,
                                             FINAL_STATUS_CANCELLED));
  EXPECT_TRUE(prerender_tracker()->TryCancelOnIOThread(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_CANCELLED));
  EXPECT_EQ(FINAL_STATUS_TIMED_OUT,
            GetCurrentStatus(kDefaultChildId, kDefaultRouteId));

  // This calls DestroyPrerenderForChildRouteIdPair().
  RunEvents();
  EXPECT_TRUE(prerender_manager()->WasPrerenderCancelled(
      kDefaultChildId, kDefaultRouteId));

  // These should all work until the prerendering RenderViewHost is destroyed.
  EXPECT_TRUE(prerender_tracker()->TryCancel(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_CANCELLED));
  EXPECT_TRUE(prerender_tracker()->TryCancelOnIOThread(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_CANCELLED));
  EXPECT_EQ(FINAL_STATUS_TIMED_OUT,
            GetCurrentStatus(kDefaultChildId, kDefaultRouteId));

  test_contents()->Cancel();
  EXPECT_TRUE(prerender_tracker()->IsPrerenderingOnIOThread(
      kDefaultChildId, kDefaultRouteId));

  // This calls RemovePrerenderOnIOThreadTask().
  RunEvents();

  FinalStatus final_status;
  EXPECT_FALSE(prerender_tracker()->GetFinalStatus(
      kDefaultChildId, kDefaultRouteId, &final_status));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(
      kDefaultChildId, kDefaultRouteId));
}

// Check that a prerendered page cancelled before it reaches the IO thread is
// handled correctly.
TEST_F(PrerenderTrackerTest, PrerenderTrackerCancelledFast) {
  test_contents()->Start();

  // Cancel the prerender.
  EXPECT_TRUE(prerender_tracker()->TryCancel(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_CANCELLED));

  EXPECT_FALSE(prerender_tracker()->TryUse(kDefaultChildId, kDefaultRouteId));
  EXPECT_TRUE(prerender_tracker()->TryCancel(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_TIMED_OUT));

  // This calls AddPrerenderOnIOThreadTask() and
  // DestroyPrerenderForChildRouteIdPair().
  RunEvents();
  EXPECT_TRUE(prerender_manager()->WasPrerenderCancelled(
      kDefaultChildId, kDefaultRouteId));

  EXPECT_TRUE(prerender_tracker()->TryCancelOnIOThread(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_TIMED_OUT));
  EXPECT_TRUE(prerender_tracker()->TryCancel(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_TIMED_OUT));
  EXPECT_EQ(FINAL_STATUS_CANCELLED, GetCurrentStatus(
      kDefaultChildId, kDefaultRouteId));

  test_contents()->Cancel();

  // This calls RemovePrerenderOnIOThreadTask().
  RunEvents();

  FinalStatus final_status;
  EXPECT_FALSE(prerender_tracker()->GetFinalStatus(
      kDefaultChildId, kDefaultRouteId, &final_status));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(
      kDefaultChildId, kDefaultRouteId));
}

// Check that handling two pages at once works.
TEST_F(PrerenderTrackerTest, PrerenderTrackerMultiple) {
  test_contents()->Start();

  // This calls AddPrerenderOnIOThreadTask().
  RunEvents();
  EXPECT_TRUE(prerender_tracker()->IsPrerenderingOnIOThread(
      kDefaultChildId, kDefaultRouteId));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(
      kDefaultChildId + 1, kDefaultRouteId + 1));
  EXPECT_FALSE(prerender_tracker()->TryUse(
      kDefaultChildId + 1, kDefaultRouteId + 1));
  EXPECT_FALSE(prerender_tracker()->TryCancel(
      kDefaultChildId + 1, kDefaultRouteId + 1, FINAL_STATUS_CANCELLED));

  // Start second prerender.
  TestPrerenderContents second_test_contents(prerender_manager(),
                                             kDefaultChildId + 1,
                                             kDefaultRouteId + 1);

  second_test_contents.Start();
  // This calls AddPrerenderOnIOThreadTask().
  RunEvents();

  // Use (kDefaultChildId, kDefaultRouteId).
  EXPECT_TRUE(prerender_tracker()->TryUse(kDefaultChildId, kDefaultRouteId));
  EXPECT_EQ(FINAL_STATUS_USED, GetCurrentStatus(
      kDefaultChildId, kDefaultRouteId));
  EXPECT_EQ(FINAL_STATUS_MAX,
            GetCurrentStatus(kDefaultChildId + 1, kDefaultRouteId + 1));

  // Cancel (kDefaultChildId + 1, kDefaultRouteId + 1).
  EXPECT_TRUE(prerender_tracker()->TryCancelOnIOThread(
      kDefaultChildId + 1, kDefaultRouteId + 1, FINAL_STATUS_CANCELLED));

  EXPECT_FALSE(prerender_tracker()->TryCancel(
      kDefaultChildId, kDefaultRouteId, FINAL_STATUS_CANCELLED));
  EXPECT_EQ(FINAL_STATUS_USED,
            GetCurrentStatus(kDefaultChildId, kDefaultRouteId));

  EXPECT_FALSE(prerender_tracker()->TryUse(
      kDefaultChildId + 1, kDefaultRouteId + 1));
  EXPECT_TRUE(prerender_tracker()->TryCancel(
      kDefaultChildId + 1, kDefaultRouteId + 1, FINAL_STATUS_CANCELLED));
  EXPECT_EQ(FINAL_STATUS_CANCELLED,
            GetCurrentStatus(kDefaultChildId + 1, kDefaultRouteId + 1));

  // This calls DestroyPrerenderForChildRouteIdPair().
  RunEvents();
  EXPECT_FALSE(prerender_manager()->WasPrerenderCancelled(kDefaultChildId,
                                                          kDefaultRouteId));
  EXPECT_TRUE(prerender_manager()->WasPrerenderCancelled(kDefaultChildId + 1,
                                                         kDefaultRouteId + 1));

  test_contents()->Cancel();
  second_test_contents.Cancel();

  // This calls RemovePrerenderOnIOThreadTask().
  RunEvents();

  FinalStatus final_status;
  EXPECT_FALSE(prerender_tracker()->GetFinalStatus(
      kDefaultChildId, kDefaultRouteId, &final_status));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(
      kDefaultChildId, kDefaultRouteId));

  EXPECT_FALSE(prerender_tracker()->GetFinalStatus(
      kDefaultChildId + 1, kDefaultRouteId + 1, &final_status));
  EXPECT_FALSE(prerender_tracker()->IsPrerenderingOnIOThread(
      kDefaultChildId + 1, kDefaultRouteId + 1));
}

}  // namespace prerender
