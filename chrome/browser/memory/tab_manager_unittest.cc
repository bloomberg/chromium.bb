// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/memory/tab_manager_web_contents_data.h"
#include "chrome/browser/memory/tab_stats.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::WebContents;
using content::WebContentsTester;

namespace memory {
namespace {

class TabStripDummyDelegate : public TestTabStripModelDelegate {
 public:
  TabStripDummyDelegate() {}

  bool RunUnloadListenerBeforeClosing(WebContents* contents) override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabStripDummyDelegate);
};

class MockTabStripModelObserver : public TabStripModelObserver {
 public:
  MockTabStripModelObserver()
      : nb_events_(0), old_contents_(nullptr), new_contents_(nullptr) {}

  int NbEvents() const { return nb_events_; }
  WebContents* OldContents() const { return old_contents_; }
  WebContents* NewContents() const { return new_contents_; }

  void Reset() {
    nb_events_ = 0;
    old_contents_ = nullptr;
    new_contents_ = nullptr;
  }

  // TabStripModelObserver implementation:
  void TabReplacedAt(TabStripModel* tab_strip_model,
                     WebContents* old_contents,
                     WebContents* new_contents,
                     int index) override {
    nb_events_++;
    old_contents_ = old_contents;
    new_contents_ = new_contents;
  }

 private:
  int nb_events_;
  WebContents* old_contents_;
  WebContents* new_contents_;

  DISALLOW_COPY_AND_ASSIGN(MockTabStripModelObserver);
};

// A mock task runner. This isn't directly a TaskRunner as the reference
// counting confuses gmock.
class LenientMockTaskRunner {
 public:
  LenientMockTaskRunner() {}
  MOCK_METHOD3(PostDelayedTask,
               bool(const tracked_objects::Location&,
                    const base::Closure&,
                    base::TimeDelta));
 private:
  DISALLOW_COPY_AND_ASSIGN(LenientMockTaskRunner);
};
using MockTaskRunner = testing::StrictMock<LenientMockTaskRunner>;

// Represents a pending task.
using Task = std::pair<base::TimeTicks, base::Closure>;

// Comparator used for sorting Task objects. Can't use std::pair's default
// comparison operators because Closure's are comparable.
struct TaskComparator {
  bool operator()(const Task& t1, const Task& t2) const {
    // Reverse the sort order so this can be used with a min-heap.
    return t1.first > t2.first;
  }
};

// A TaskRunner implementation that delegates to a MockTaskRunner for asserting
// on task insertion order, and which stores tasks for actual later execution.
class TaskRunnerProxy : public base::TaskRunner {
 public:
  TaskRunnerProxy(MockTaskRunner* mock,
                  base::SimpleTestTickClock* clock)
      : mock_(mock), clock_(clock) {}
  bool RunsTasksOnCurrentThread() const override { return true; }
  bool PostDelayedTask(const tracked_objects::Location& location,
                       const base::Closure& closure,
                       base::TimeDelta delta) override {
    mock_->PostDelayedTask(location, closure, delta);
    base::TimeTicks when = clock_->NowTicks() + delta;
    tasks_.push_back(Task(when, closure));
    // Use 'greater' comparator to make this a min heap.
    std::push_heap(tasks_.begin(), tasks_.end(), TaskComparator());
    return true;
  }

  // Runs tasks up to the current time. Returns the number of tasks executed.
  size_t RunTasks() {
    base::TimeTicks now = clock_->NowTicks();
    size_t count = 0;
    while (!tasks_.empty() && tasks_.front().first <= now) {
      tasks_.front().second.Run();
      std::pop_heap(tasks_.begin(), tasks_.end(), TaskComparator());
      tasks_.pop_back();
      ++count;
    }
    return count;
  }

  // Advances the clock and runs the next task in the queue. Can run more than
  // one task if multiple tasks have the same scheduled time.
  size_t RunNextTask() {
    // Get the time of the next task that can possibly run.
    base::TimeTicks when = tasks_.front().first;

    // Advance the clock to exactly that time.
    base::TimeTicks now = clock_->NowTicks();
    if (now < when) {
      base::TimeDelta delta = when - now;
      clock_->Advance(delta);
    }

    // Run whatever tasks are now eligible to run.
    return RunTasks();
  }

  size_t size() const { return tasks_.size(); }

 private:
  ~TaskRunnerProxy() override {}

  MockTaskRunner* mock_;
  base::SimpleTestTickClock* clock_;

  // A min-heap of outstanding tasks.
  using Task = std::pair<base::TimeTicks, base::Closure>;
  std::vector<Task> tasks_;

  DISALLOW_COPY_AND_ASSIGN(TaskRunnerProxy);
};

enum TestIndicies {
  kSelected,
  kPinned,
  kApp,
  kPlayingAudio,
  kFormEntry,
  kRecent,
  kOld,
  kReallyOld,
  kOldButPinned,
  kInternalPage,
};
}  // namespace

class LenientTabManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  WebContents* CreateWebContents() {
    return WebContents::Create(WebContents::CreateParams(profile()));
  }

  MOCK_METHOD2(NotifyRendererProcess,
               void(const content::RenderProcessHost*,
                    base::MemoryPressureListener::MemoryPressureLevel));
};
using TabManagerTest = testing::StrictMock<LenientTabManagerTest>;

// TODO(georgesak): Add tests for protection to tabs with form input and
// playing audio;

// Tests the sorting comparator to make sure it's producing the desired order.
TEST_F(TabManagerTest, Comparator) {
  TabStatsList test_list;
  const base::TimeTicks now = base::TimeTicks::Now();

  // Add kSelected last to verify that the array is being sorted.

  {
    TabStats stats;
    stats.last_active = now;
    stats.is_pinned = true;
    stats.child_process_host_id = kPinned;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now;
    stats.is_app = true;
    stats.child_process_host_id = kApp;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now;
    stats.is_media = true;
    stats.child_process_host_id = kPlayingAudio;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now;
    stats.has_form_entry = true;
    stats.child_process_host_id = kFormEntry;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now - base::TimeDelta::FromSeconds(10);
    stats.child_process_host_id = kRecent;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now - base::TimeDelta::FromMinutes(15);
    stats.child_process_host_id = kOld;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now - base::TimeDelta::FromDays(365);
    stats.child_process_host_id = kReallyOld;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.is_pinned = true;
    stats.last_active = now - base::TimeDelta::FromDays(365);
    stats.child_process_host_id = kOldButPinned;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now;
    stats.is_internal_page = true;
    stats.child_process_host_id = kInternalPage;
    test_list.push_back(stats);
  }

  // This entry sorts to the front, so by adding it last, it verifies that the
  // array is being sorted.
  {
    TabStats stats;
    stats.last_active = now;
    stats.is_selected = true;
    stats.child_process_host_id = kSelected;
    test_list.push_back(stats);
  }

  std::sort(test_list.begin(), test_list.end(), TabManager::CompareTabStats);

  int index = 0;
  EXPECT_EQ(kSelected, test_list[index++].child_process_host_id);
  EXPECT_EQ(kFormEntry, test_list[index++].child_process_host_id);
  EXPECT_EQ(kPlayingAudio, test_list[index++].child_process_host_id);
  EXPECT_EQ(kPinned, test_list[index++].child_process_host_id);
  EXPECT_EQ(kOldButPinned, test_list[index++].child_process_host_id);
  EXPECT_EQ(kApp, test_list[index++].child_process_host_id);
  EXPECT_EQ(kRecent, test_list[index++].child_process_host_id);
  EXPECT_EQ(kOld, test_list[index++].child_process_host_id);
  EXPECT_EQ(kReallyOld, test_list[index++].child_process_host_id);
  EXPECT_EQ(kInternalPage, test_list[index++].child_process_host_id);
}

TEST_F(TabManagerTest, IsInternalPage) {
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUIDownloadsURL)));
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUIHistoryURL)));
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUINewTabURL)));
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUISettingsURL)));

// Debugging URLs are not included.
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(TabManager::IsInternalPage(GURL(chrome::kChromeUIDiscardsURL)));
#endif
  EXPECT_FALSE(
      TabManager::IsInternalPage(GURL(chrome::kChromeUINetInternalsURL)));

  // Prefix matches are included.
  EXPECT_TRUE(
      TabManager::IsInternalPage(GURL("chrome://settings/fakeSetting")));
}

// Ensures discarding tabs leaves TabStripModel in a good state.
TEST_F(TabManagerTest, DiscardWebContentsAt) {
  TabManager tab_manager;

  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  tabstrip.AddObserver(&tab_manager);

  // Fill it with some tabs.
  WebContents* contents1 = CreateWebContents();
  tabstrip.AppendWebContents(contents1, true);
  WebContents* contents2 = CreateWebContents();
  tabstrip.AppendWebContents(contents2, true);

  // Start watching for events after the appends to avoid observing state
  // transitions that aren't relevant to this test.
  MockTabStripModelObserver tabstrip_observer;
  tabstrip.AddObserver(&tabstrip_observer);

  // Discard one of the tabs.
  WebContents* null_contents1 = tab_manager.DiscardWebContentsAt(0, &tabstrip);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_TRUE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(0)));
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(1)));
  ASSERT_EQ(null_contents1, tabstrip.GetWebContentsAt(0));
  ASSERT_EQ(contents2, tabstrip.GetWebContentsAt(1));
  ASSERT_EQ(1, tabstrip_observer.NbEvents());
  EXPECT_EQ(contents1, tabstrip_observer.OldContents());
  EXPECT_EQ(null_contents1, tabstrip_observer.NewContents());
  tabstrip_observer.Reset();

  // Discard the same tab again, after resetting its discard state.
  tab_manager.GetWebContentsData(tabstrip.GetWebContentsAt(0))
      ->SetDiscardState(false);
  WebContents* null_contents2 = tab_manager.DiscardWebContentsAt(0, &tabstrip);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_TRUE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(0)));
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(1)));
  ASSERT_EQ(null_contents2, tabstrip.GetWebContentsAt(0));
  ASSERT_EQ(contents2, tabstrip.GetWebContentsAt(1));
  ASSERT_EQ(1, tabstrip_observer.NbEvents());
  EXPECT_EQ(null_contents1, tabstrip_observer.OldContents());
  EXPECT_EQ(null_contents2, tabstrip_observer.NewContents());

  // Activating the tab should clear its discard state.
  tabstrip.ActivateTabAt(0, true /* user_gesture */);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(0)));
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(1)));

  // Don't discard active tab.
  tab_manager.DiscardWebContentsAt(0, &tabstrip);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(0)));
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(1)));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Makes sure that reloading a discarded tab without activating it unmarks the
// tab as discarded so it won't reload on activation.
TEST_F(TabManagerTest, ReloadDiscardedTabContextMenu) {
  // Note that we do not add |tab_manager| as an observer to |tabstrip| here as
  // the event we are trying to test for is not related to the tab strip, but
  // the web content instead and therefore should be handled by WebContentsData
  // (which observes the web content).
  TabManager tab_manager;
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());

  // Create 2 tabs because the active tab cannot be discarded.
  tabstrip.AppendWebContents(CreateWebContents(), true);
  content::WebContents* test_contents =
      WebContentsTester::CreateTestWebContents(browser_context(), nullptr);
  tabstrip.AppendWebContents(test_contents, false);  // Opened in background.

  // Navigate to a web page. This is necessary to set a current entry in memory
  // so the reload can happen.
  WebContentsTester::For(test_contents)
      ->NavigateAndCommit(GURL("chrome://newtab"));
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(1)));

  tab_manager.DiscardWebContentsAt(1, &tabstrip);
  EXPECT_TRUE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(1)));

  tabstrip.GetWebContentsAt(1)->GetController().Reload(false);
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(1)));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Makes sure that the last active time property is saved even though the tab is
// discarded.
TEST_F(TabManagerTest, DiscardedTabKeepsLastActiveTime) {
  TabManager tab_manager;
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  tabstrip.AddObserver(&tab_manager);

  tabstrip.AppendWebContents(CreateWebContents(), true);
  WebContents* test_contents = CreateWebContents();
  tabstrip.AppendWebContents(test_contents, false);

  // Simulate an old inactive tab about to get discarded.
  base::TimeTicks new_last_active_time =
      base::TimeTicks::Now() - base::TimeDelta::FromMinutes(35);
  test_contents->SetLastActiveTime(new_last_active_time);
  EXPECT_EQ(new_last_active_time, test_contents->GetLastActiveTime());

  WebContents* null_contents = tab_manager.DiscardWebContentsAt(1, &tabstrip);
  EXPECT_EQ(new_last_active_time, null_contents->GetLastActiveTime());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

namespace {

using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

// Function that always indicates the absence of memory pressure.
MemoryPressureLevel ReturnNoPressure() {
  return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

// Function that simply parrots back an externally specified memory pressure
// level.
MemoryPressureLevel ReturnSpecifiedPressure(
    const MemoryPressureLevel* level) {
  return *level;
}

}  // namespace

// Ensure that memory pressure notifications are forwarded to child processes.
TEST_F(TabManagerTest, ChildProcessNotifications) {
  TabManager tm;

  // Set up the tab strip.
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());

  // Create test clock, task runner.
  base::SimpleTestTickClock test_clock;
  MockTaskRunner mock_task_runner;
  scoped_refptr<TaskRunnerProxy> task_runner(
      new TaskRunnerProxy(&mock_task_runner, &test_clock));

  // Configure the TabManager for testing.
  tabstrip.AddObserver(&tm);
  tm.test_tab_strip_models_.push_back(
      TabManager::TestTabStripModel(&tabstrip, false /* !is_app */));
  tm.test_tick_clock_ = &test_clock;
  tm.task_runner_ = task_runner;
  tm.notify_renderer_process_ = base::Bind(
      &TabManagerTest::NotifyRendererProcess, base::Unretained(this));

  // Create two dummy tabs.
  auto tab1 = CreateWebContents();
  auto tab2 = CreateWebContents();
  tabstrip.AppendWebContents(tab1, true);   // Foreground tab.
  tabstrip.AppendWebContents(tab2, false);  // Opened in background.
  const content::RenderProcessHost* renderer1 = tab1->GetRenderProcessHost();
  const content::RenderProcessHost* renderer2 = tab2->GetRenderProcessHost();

  // Expect that the tab manager has not yet encountered memory pressure.
  EXPECT_FALSE(tm.under_memory_pressure_);
  EXPECT_EQ(0u, tm.notified_renderers_.size());

  // Simulate a memory pressure situation that will immediately end. This should
  // cause no notifications or scheduled tasks.
  auto level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  tm.get_current_pressure_level_ = base::Bind(&ReturnNoPressure);
  tm.OnMemoryPressure(level);
  testing::Mock::VerifyAndClearExpectations(&mock_task_runner);
  EXPECT_FALSE(tm.under_memory_pressure_);
  EXPECT_EQ(0u, task_runner->size());
  EXPECT_EQ(0u, tm.notified_renderers_.size());

  // START OF MEMORY PRESSURE

  // Simulate a memory pressure situation that persists. This should cause a
  // task to be scheduled.
  tm.get_current_pressure_level_ = base::Bind(
      &ReturnSpecifiedPressure, base::Unretained(&level));
  EXPECT_CALL(mock_task_runner, PostDelayedTask(
      testing::_,
      testing::_,
      base::TimeDelta::FromSeconds(tm.kRendererNotificationDelayInSeconds)));
  EXPECT_CALL(*this, NotifyRendererProcess(renderer2, level));
  tm.OnMemoryPressure(level);
  testing::Mock::VerifyAndClearExpectations(&mock_task_runner);
  testing::Mock::VerifyAndClearExpectations(this);
  EXPECT_TRUE(tm.under_memory_pressure_);
  EXPECT_EQ(1u, task_runner->size());
  EXPECT_EQ(1u, tm.notified_renderers_.size());
  EXPECT_EQ(1u, tm.notified_renderers_.count(renderer2));

  // REPEATED MEMORY PRESSURE SIGNAL

  // Simulate another memory pressure event. This should not cause any
  // additional tasks to be scheduled, nor any further renderer notifications.
  tm.OnMemoryPressure(level);
  testing::Mock::VerifyAndClearExpectations(&mock_task_runner);
  testing::Mock::VerifyAndClearExpectations(this);
  EXPECT_TRUE(tm.under_memory_pressure_);
  EXPECT_EQ(1u, task_runner->size());
  EXPECT_EQ(1u, tm.notified_renderers_.size());

  // FIRST SCHEDULED NOTIFICATION

  // Run the scheduled task. This should cause another notification, but this
  // time to the foreground tab. It should also cause another scheduled event.
  EXPECT_CALL(mock_task_runner, PostDelayedTask(
      testing::_,
      testing::_,
      base::TimeDelta::FromSeconds(tm.kRendererNotificationDelayInSeconds)));
  EXPECT_CALL(*this, NotifyRendererProcess(renderer1, level));
  EXPECT_EQ(1u, task_runner->RunNextTask());
  testing::Mock::VerifyAndClearExpectations(&mock_task_runner);
  testing::Mock::VerifyAndClearExpectations(this);
  EXPECT_TRUE(tm.under_memory_pressure_);
  EXPECT_EQ(1u, task_runner->size());
  EXPECT_EQ(2u, tm.notified_renderers_.size());
  EXPECT_EQ(1u, tm.notified_renderers_.count(renderer1));

  // SECOND SCHEDULED NOTIFICATION

  // Run the scheduled task. This should cause another notification, to the
  // background tab. It should also cause another scheduled event.
  EXPECT_CALL(mock_task_runner, PostDelayedTask(
      testing::_,
      testing::_,
      base::TimeDelta::FromSeconds(tm.kRendererNotificationDelayInSeconds)));
  EXPECT_CALL(*this, NotifyRendererProcess(renderer2, level));
  EXPECT_EQ(1u, task_runner->RunNextTask());
  testing::Mock::VerifyAndClearExpectations(&mock_task_runner);
  testing::Mock::VerifyAndClearExpectations(this);
  EXPECT_TRUE(tm.under_memory_pressure_);
  EXPECT_EQ(1u, task_runner->size());
  EXPECT_EQ(1u, tm.notified_renderers_.size());

  // Expect that the background tab has been notified. The list of notified
  // renderers maintained by the TabManager should also have been reset because
  // the notification logic restarts after all renderers are notified.
  EXPECT_EQ(1u, tm.notified_renderers_.count(renderer2));

  // END OF MEMORY PRESSURE

  // End the memory pressure condition and run the scheduled event. This should
  // clean up the various state data. It should not schedule another event nor
  // fire any notifications.
  level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  EXPECT_EQ(1u, task_runner->RunNextTask());
  testing::Mock::VerifyAndClearExpectations(&mock_task_runner);
  testing::Mock::VerifyAndClearExpectations(this);
  EXPECT_FALSE(tm.under_memory_pressure_);
  EXPECT_EQ(0u, task_runner->size());
  EXPECT_EQ(0u, tm.notified_renderers_.size());


  // Clean up the tabstrip.
  tabstrip.CloseAllTabs();
  ASSERT_TRUE(tabstrip.empty());
}

}  // namespace memory
