// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_IOS_CHROME_BROWSING_DATA_REMOVER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_IOS_CHROME_BROWSING_DATA_REMOVER_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/prefs/pref_member.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"

namespace ios {
class ChromeBrowserState;
}

namespace net {
class URLRequestContextGetter;
}

// IOSChromeBrowsingDataRemover is responsible for removing data related to
// browsing: visits in url database, downloads, cookies ...
class IOSChromeBrowsingDataRemover {
 public:
  // Creates a IOSChromeBrowsingDataRemover to remove browser data from the
  // specified ChromeBrowserstate. Use Remove to initiate the removal.
  explicit IOSChromeBrowsingDataRemover(ios::ChromeBrowserState* browser_state);
  ~IOSChromeBrowsingDataRemover();

  // Is the object currently in the process of removing data?
  bool is_removing() { return is_removing_; }

  // Removes browsing data for the given |time_range| with data types specified
  // by |remove_mask|. The |callback| is invoked asynchronously when the data
  // has been removed.
  void Remove(browsing_data::TimePeriod time_period,
              BrowsingDataRemoveMask remove_mask,
              base::OnceClosure callback);

 private:
  // Represents a single removal task. Contains all parameters to execute it.
  struct RemovalTask {
    RemovalTask(base::Time delete_begin,
                base::Time delete_end,
                BrowsingDataRemoveMask mask,
                base::OnceClosure callback);
    RemovalTask(RemovalTask&& other) noexcept;
    ~RemovalTask();

    base::Time delete_begin;
    base::Time delete_end;
    BrowsingDataRemoveMask mask;
    base::OnceClosure callback;
  };

  // Setter for |is_removing_|; DCHECKs that we can only start removing if we're
  // not already removing, and vice-versa.
  void SetRemoving(bool is_removing);

  // Callback for when TemplateURLService has finished loading. Clears the data,
  // and invoke the callback.
  void OnKeywordsLoaded(base::Time delete_begin,
                        base::Time delete_end,
                        base::OnceClosure callback);

  // Execute the next removal task. Called after the previous task was finished
  // or directly from Remove.
  void RunNextTask();

  // Removes the specified items related to browsing.
  void RemoveImpl(base::Time delete_begin,
                  base::Time delete_end,
                  BrowsingDataRemoveMask mask);

  // Invokes the current task callback that the removal has completed.
  void NotifyRemovalComplete();

  // Called by the closures returned by CreatePendingTaskCompletionClosure().
  // Checks if all tasks have completed, and if so, calls Notify().
  void OnTaskComplete();

  // Increments the number of pending tasks by one, and returns a OnceClosure
  // that calls OnTaskComplete(). The Remover is complete once all the closures
  // created by this method have been invoked.
  base::OnceClosure CreatePendingTaskCompletionClosure();

  // Returns a weak pointer to IOSChromeBrowsingDataRemover for internal
  // purposes.
  base::WeakPtr<IOSChromeBrowsingDataRemover> GetWeakPtr();

  // This object is sequence affine.
  SEQUENCE_CHECKER(sequence_checker_);

  // ChromeBrowserState we're to remove from.
  ios::ChromeBrowserState* browser_state_ = nullptr;

  // Used to delete data from HTTP cache.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // Is the object currently in the process of removing data?
  bool is_removing_ = false;

  // Number of pending tasks.
  int pending_tasks_count_ = 0;

  // Removal tasks to be processed.
  base::queue<RemovalTask> removal_queue_;

  // Used if we need to clear history.
  base::CancelableTaskTracker history_task_tracker_;

  std::unique_ptr<TemplateURLService::Subscription> template_url_subscription_;

  base::WeakPtrFactory<IOSChromeBrowsingDataRemover> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeBrowsingDataRemover);
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_IOS_CHROME_BROWSING_DATA_REMOVER_H_
