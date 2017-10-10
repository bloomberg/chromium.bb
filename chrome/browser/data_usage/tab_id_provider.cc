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
typedef base::OnceCallback<int32_t(void)> TabIdGetter;
typedef base::OnceCallback<void(int32_t)> TabIdCallback;

}  // namespace

// Object that can run a list of callbacks that take tab IDs. New callbacks
// can only be added before it starts running the callbacks. Callbacks added
// will each be run exactly once.
class TabIdProvider::CallbackRunner {
 public:
  CallbackRunner() : is_done_(false), weak_ptr_factory_(this) {}

  // Adds a new callback to be run later. New callbacks must not be added after
  // RunAll has been called, since they will never be run, as RunAll() is only
  // ever called once.
  void AddCallback(TabIdCallback callback) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!is_done_);
    callbacks_.push_back(std::move(callback));
  }

  // Runs all the callbacks in the order that they were added. This method must
  // not be called more than once.
  void RunAll(int32_t tab_info) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!is_done_);
    is_done_ = true;

    std::vector<TabIdCallback> running_callbacks;
    running_callbacks.swap(callbacks_);

    for (auto& callback : running_callbacks)
      std::move(callback).Run(tab_info);
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
                             const base::Location& from_here,
                             TabIdGetter tab_id_getter)
    : is_tab_info_ready_(false), tab_info_(-1), weak_ptr_factory_(this) {
  std::unique_ptr<CallbackRunner> callback_runner(new CallbackRunner());
  weak_callback_runner_ = callback_runner->GetWeakPtr();
  callback_runner->AddCallback(
      base::BindOnce(&TabIdProvider::OnTabIdReady, GetWeakPtr()));

  // The posted task takes ownership of |callback_runner|. If the task fails to
  // be posted, then the destructor of |callback_runner| will pass a tab ID of
  // -1 to OnTabIdReady, so that case doesn't need to be explicitly handled
  // here.
  base::PostTaskAndReplyWithResult(
      task_runner, from_here, std::move(tab_id_getter),
      base::BindOnce(&CallbackRunner::RunAll,
                     base::Owned(callback_runner.release())));
}

TabIdProvider::~TabIdProvider() {}

void TabIdProvider::ProvideTabId(TabIdCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_tab_info_ready_) {
    std::move(callback).Run(tab_info_);
    return;
  }
  if (weak_callback_runner_) {
    weak_callback_runner_->AddCallback(std::move(callback));
    return;
  }
  // If no cached tab ID is available and |weak_callback_runner_| has been
  // destroyed, pass a tab ID of -1 to the callback indicating that no tab was
  // found.
  std::move(callback).Run(-1);
}

base::WeakPtr<TabIdProvider> TabIdProvider::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

// static
const void* const TabIdProvider::kTabIdProviderUserDataKey =
    "TabIdProviderUserDataKey";

void TabIdProvider::OnTabIdReady(int32_t tab_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!is_tab_info_ready_);

  tab_info_ = tab_info;
  is_tab_info_ready_ = true;
}

}  // namespace chrome_browser_data_usage
