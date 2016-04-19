// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_usage/tab_id_provider.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"

namespace chrome_browser_data_usage {

namespace {

// Convenience typedefs for clarity.
typedef base::Callback<int32_t(void)> TabIdGetter;
typedef base::Callback<void(int32_t)> TabIdCallback;

}  // namespace

// Object that can run a list of callbacks that take tab IDs. New callbacks
// can only be added before it starts running the callbacks. Callbacks added
// will each be run exactly once.
class TabIdProvider::CallbackRunner {
 public:
  CallbackRunner() : is_done_(false), weak_ptr_factory_(this) {}

  ~CallbackRunner() {
    // Ensure that no callbacks are abandoned without being run.
    if (!is_done_)
      RunAll(-1);
  }

  // Adds a new callback to be run later. New callbacks must not be added after
  // RunAll has been called.
  void AddCallback(const TabIdCallback& callback) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!is_done_);
    callbacks_.push_back(callback);
  }

  // Runs all the callbacks in the order that they were added. This method must
  // not be called more than once.
  void RunAll(int32_t tab_id) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!is_done_);
    is_done_ = true;
    // It's safe to run the |callbacks_| while iterating through them because
    // |callbacks_| can no longer be modified at this point, so it's impossible
    // for one of the |callbacks_| to add more callbacks to this CallbackRunner.
    for (const auto& callback : callbacks_)
      callback.Run(tab_id);
    callbacks_.clear();
  }

  base::WeakPtr<CallbackRunner> GetWeakPtr() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::ThreadChecker thread_checker_;
  bool is_done_;
  std::vector<TabIdCallback> callbacks_;
  base::WeakPtrFactory<CallbackRunner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CallbackRunner);
};

TabIdProvider::TabIdProvider(base::TaskRunner* task_runner,
                             const tracked_objects::Location& from_here,
                             const TabIdGetter& tab_id_getter)
    : is_tab_id_ready_(false), tab_id_(-1), weak_ptr_factory_(this) {
  std::unique_ptr<CallbackRunner> callback_runner(new CallbackRunner());
  weak_callback_runner_ = callback_runner->GetWeakPtr();
  callback_runner->AddCallback(
      base::Bind(&TabIdProvider::OnTabIdReady, GetWeakPtr()));

  // The posted task takes ownership of |callback_runner|. If the task fails to
  // be posted, then the destructor of |callback_runner| will pass a tab ID of
  // -1 to OnTabIdReady, so that case doesn't need to be explicitly handled
  // here.
  base::PostTaskAndReplyWithResult(
      task_runner, from_here, tab_id_getter,
      base::Bind(&CallbackRunner::RunAll,
                 base::Owned(callback_runner.release())));
}

TabIdProvider::~TabIdProvider() {}

void TabIdProvider::ProvideTabId(const TabIdCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_tab_id_ready_) {
    callback.Run(tab_id_);
    return;
  }
  if (weak_callback_runner_) {
    weak_callback_runner_->AddCallback(callback);
    return;
  }
  // If no cached tab ID is available and |weak_callback_runner_| has been
  // destroyed, pass a tab ID of -1 to the callback indicating that no tab was
  // found.
  callback.Run(-1);
}

base::WeakPtr<TabIdProvider> TabIdProvider::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

// static
const void* TabIdProvider::kUserDataKey =
    static_cast<void*>(&TabIdProvider::kUserDataKey);

void TabIdProvider::OnTabIdReady(int32_t tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!is_tab_id_ready_);
  tab_id_ = tab_id;
  is_tab_id_ready_ = true;
}

}  // namespace chrome_browser_data_usage
