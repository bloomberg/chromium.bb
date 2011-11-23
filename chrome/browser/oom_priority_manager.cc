// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/oom_priority_manager.h"

#include <algorithm>
#include <vector>

#include "base/process.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_constants.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/zygote_host_linux.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

#if !defined(OS_CHROMEOS)
#error This file only meant to be compiled on ChromeOS
#endif

using base::TimeDelta;
using base::TimeTicks;
using base::ProcessHandle;
using base::ProcessMetrics;
using content::BrowserThread;

namespace {

// Returns a unique ID for a TabContents.  Do not cast back to a pointer, as
// the TabContents could be deleted if the user closed the tab.
int64 IdFromTabContents(TabContents* tab_contents) {
  return reinterpret_cast<int64>(tab_contents);
}

// Discards a tab with the given unique ID.  Returns true if discard occurred.
bool DiscardTabById(int64 target_tab_contents_id) {
  for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
       browser_iterator != BrowserList::end(); ++browser_iterator) {
    Browser* browser = *browser_iterator;
    TabStripModel* model = browser->tabstrip_model();
    for (int idx = 0; idx < model->count(); idx++) {
      // Can't discard tabs that are already discarded.
      if (model->IsTabDiscarded(idx))
        continue;
      TabContents* tab_contents = model->GetTabContentsAt(idx)->tab_contents();
      int64 tab_contents_id = IdFromTabContents(tab_contents);
      if (tab_contents_id == target_tab_contents_id) {
        model->DiscardTabContentsAt(idx);
        return true;
      }
    }
  }
  return false;
}

}  // namespace

namespace browser {

// The default interval in seconds after which to adjust the oom_score_adj
// value.
#define ADJUSTMENT_INTERVAL_SECONDS 10

// The default interval in milliseconds to wait before setting the score of
// currently focused tab.
#define FOCUSED_TAB_SCORE_ADJUST_INTERVAL_MS 500

OomPriorityManager::TabStats::TabStats()
  : is_pinned(false),
    is_selected(false),
    renderer_handle(0),
    tab_contents_id(0) {
}

OomPriorityManager::TabStats::~TabStats() {
}

OomPriorityManager::OomPriorityManager()
  : focused_tab_pid_(0) {
  registrar_.Add(this,
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
      content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
      content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      content::NotificationService::AllBrowserContextsAndSources());
}

OomPriorityManager::~OomPriorityManager() {
  Stop();
}

void OomPriorityManager::Start() {
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE,
                 TimeDelta::FromSeconds(ADJUSTMENT_INTERVAL_SECONDS),
                 this,
                 &OomPriorityManager::AdjustOomPriorities);
  }
}

void OomPriorityManager::Stop() {
  timer_.Stop();
}

std::vector<string16> OomPriorityManager::GetTabTitles() {
  TabStatsList stats = GetTabStatsOnUIThread();
  base::AutoLock pid_to_oom_score_autolock(pid_to_oom_score_lock_);
  std::vector<string16> titles;
  titles.reserve(stats.size());
  TabStatsList::iterator it = stats.begin();
  for ( ; it != stats.end(); ++it) {
    string16 str = it->title;
    str += ASCIIToUTF16(" (");
    int score = pid_to_oom_score_[it->renderer_handle];
    str += base::IntToString16(score);
    str += ASCIIToUTF16(")");
    titles.push_back(str);
  }
  return titles;
}

// TODO(jamescook): This should consider tabs with references to other tabs,
// such as tabs created with JavaScript window.open().  We might want to
// discard the entire set together, or use that in the priority computation.
bool OomPriorityManager::DiscardTab() {
  TabStatsList stats = GetTabStatsOnUIThread();
  if (stats.empty())
    return false;
  // Loop until we find a non-discarded tab to kill.
  for (TabStatsList::const_reverse_iterator stats_rit = stats.rbegin();
       stats_rit != stats.rend();
       ++stats_rit) {
    int64 least_important_tab_id = stats_rit->tab_contents_id;
    if (DiscardTabById(least_important_tab_id))
      return true;
  }
  return false;
}

// Returns true if |first| is considered less desirable to be killed
// than |second|.
bool OomPriorityManager::CompareTabStats(TabStats first,
                                         TabStats second) {
  // Being currently selected is most important.
  if (first.is_selected != second.is_selected)
    return first.is_selected == true;

  // Being pinned is second most important.
  if (first.is_pinned != second.is_pinned)
    return first.is_pinned == true;

  // Being more recently selected is more important.
  return first.last_selected > second.last_selected;
}

void OomPriorityManager::AdjustFocusedTabScoreOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::AutoLock pid_to_oom_score_autolock(pid_to_oom_score_lock_);
  ZygoteHost::GetInstance()->AdjustRendererOOMScore(
      focused_tab_pid_, chrome::kLowestRendererOomScore);
  pid_to_oom_score_[focused_tab_pid_] = chrome::kLowestRendererOomScore;
}

void OomPriorityManager::OnFocusTabScoreAdjustmentTimeout() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &OomPriorityManager::AdjustFocusedTabScoreOnFileThread));
}

void OomPriorityManager::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  base::ProcessHandle handle = 0;
  base::AutoLock pid_to_oom_score_autolock(pid_to_oom_score_lock_);
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      handle =
          content::Details<content::RenderProcessHost::RendererClosedDetails>(
              details)->handle;
      pid_to_oom_score_.erase(handle);
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      handle = content::Source<content::RenderProcessHost>(source)->
          GetHandle();
      pid_to_oom_score_.erase(handle);
      break;
    }
    case content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED: {
      bool visible = *content::Details<bool>(details).ptr();
      if (visible) {
        focused_tab_pid_ = content::Source<RenderWidgetHost>(source).ptr()->
            process()->GetHandle();

        // If the currently focused tab already has a lower score, do not
        // set it. This can happen in case the newly focused tab is script
        // connected to the previous tab.
        ProcessScoreMap::iterator it;
        it = pid_to_oom_score_.find(focused_tab_pid_);
        if (it == pid_to_oom_score_.end()
            || it->second != chrome::kLowestRendererOomScore) {
          // By starting a timer we guarantee that the tab is focused for
          // certain amount of time. Secondly, it also does not add overhead
          // to the tab switching time.
          if (focus_tab_score_adjust_timer_.IsRunning())
            focus_tab_score_adjust_timer_.Reset();
          else
            focus_tab_score_adjust_timer_.Start(FROM_HERE,
              TimeDelta::FromMilliseconds(FOCUSED_TAB_SCORE_ADJUST_INTERVAL_MS),
              this, &OomPriorityManager::OnFocusTabScoreAdjustmentTimeout);
        }
      }
      break;
    }
    default:
      NOTREACHED() << L"Received unexpected notification";
      break;
  }
}

// Here we collect most of the information we need to sort the
// existing renderers in priority order, and hand out oom_score_adj
// scores based on that sort order.
//
// Things we need to collect on the browser thread (because
// TabStripModel isn't thread safe):
// 1) whether or not a tab is pinned
// 2) last time a tab was selected
// 3) is the tab currently selected
void OomPriorityManager::AdjustOomPriorities() {
  if (BrowserList::size() == 0)
    return;
  TabStatsList stats_list = GetTabStatsOnUIThread();
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &OomPriorityManager::AdjustOomPrioritiesOnFileThread,
                        stats_list));
}

OomPriorityManager::TabStatsList OomPriorityManager::GetTabStatsOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TabStatsList stats_list;
  stats_list.reserve(32);  // 99% of users have < 30 tabs open
  for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
       browser_iterator != BrowserList::end(); ++browser_iterator) {
    Browser* browser = *browser_iterator;
    const TabStripModel* model = browser->tabstrip_model();
    for (int i = 0; i < model->count(); i++) {
      TabContents* contents = model->GetTabContentsAt(i)->tab_contents();
      if (!contents->is_crashed()) {
        TabStats stats;
        stats.last_selected = contents->last_selected_time();
        stats.renderer_handle = contents->GetRenderProcessHost()->GetHandle();
        stats.is_pinned = model->IsTabPinned(i);
        stats.is_selected = model->IsTabSelected(i);
        stats.title = contents->GetTitle();
        stats.tab_contents_id = IdFromTabContents(contents);
        stats_list.push_back(stats);
      }
    }
  }
  // Sort the data we collected so that least desirable to be
  // killed is first, most desirable is last.
  std::sort(stats_list.begin(), stats_list.end(), CompareTabStats);
  return stats_list;
}

void OomPriorityManager::AdjustOomPrioritiesOnFileThread(
    TabStatsList stats_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::AutoLock pid_to_oom_score_autolock(pid_to_oom_score_lock_);

  // Now we assign priorities based on the sorted list.  We're
  // assigning priorities in the range of kLowestRendererOomScore to
  // kHighestRendererOomScore (defined in chrome_constants.h).
  // oom_score_adj takes values from -1000 to 1000.  Negative values
  // are reserved for system processes, and we want to give some room
  // below the range we're using to allow for things that want to be
  // above the renderers in priority, so the defined range gives us
  // some variation in priority without taking up the whole range.  In
  // the end, however, it's a pretty arbitrary range to use.  Higher
  // values are more likely to be killed by the OOM killer.
  //
  // We also remove any duplicate PIDs, leaving the most important
  // (least likely to be killed) of the duplicates, so that a
  // particular renderer process takes on the oom_score_adj of the
  // least likely tab to be killed.
  const int kPriorityRange = chrome::kHighestRendererOomScore -
                             chrome::kLowestRendererOomScore;
  float priority_increment =
      static_cast<float>(kPriorityRange) / stats_list.size();
  float priority = chrome::kLowestRendererOomScore;
  std::set<base::ProcessHandle> already_seen;
  int score = 0;
  ProcessScoreMap::iterator it;
  for (TabStatsList::iterator iterator = stats_list.begin();
       iterator != stats_list.end(); ++iterator) {
    if (already_seen.find(iterator->renderer_handle) == already_seen.end()) {
      already_seen.insert(iterator->renderer_handle);
      // If a process has the same score as the newly calculated value,
      // do not set it.
      score = static_cast<int>(priority + 0.5f);
      it = pid_to_oom_score_.find(iterator->renderer_handle);
      if (it == pid_to_oom_score_.end() || it->second != score) {
        ZygoteHost::GetInstance()->AdjustRendererOOMScore(
            iterator->renderer_handle, score);
        pid_to_oom_score_[iterator->renderer_handle] = score;
      }
      priority += priority_increment;
    }
  }
}

}  // namespace browser
