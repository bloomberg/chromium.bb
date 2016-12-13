// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/issue_manager.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"

namespace media_router {

IssueManager::IssueManager() : top_issue_(nullptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

IssueManager::~IssueManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void IssueManager::AddIssue(const IssueInfo& issue_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = std::find_if(issues_.begin(), issues_.end(),
                         [&issue_info](const std::unique_ptr<Issue>& issue) {
                           return issue_info == issue->info();
                         });
  if (it != issues_.end())
    return;

  issues_.push_back(base::MakeUnique<Issue>(issue_info));
  MaybeUpdateTopIssue();
}

void IssueManager::ClearIssue(const Issue::Id& issue_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issues_.erase(
      std::remove_if(issues_.begin(), issues_.end(),
                     [&issue_id](const std::unique_ptr<Issue>& issue) {
                       return issue_id == issue->id();
                     }),
      issues_.end());
  MaybeUpdateTopIssue();
}

void IssueManager::RegisterObserver(IssuesObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  DCHECK(!issues_observers_.HasObserver(observer));

  issues_observers_.AddObserver(observer);
  MaybeUpdateTopIssue();
  if (top_issue_)
    observer->OnIssue(*top_issue_);
}

void IssueManager::UnregisterObserver(IssuesObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issues_observers_.RemoveObserver(observer);
}

void IssueManager::MaybeUpdateTopIssue() {
  const Issue* new_top_issue = nullptr;
  if (!issues_.empty()) {
    // Select the first blocking issue in the list of issues.
    // If there are none, simply select the first issue in the list.
    auto it = std::find_if(issues_.begin(), issues_.end(),
                           [](const std::unique_ptr<Issue>& issue) {
                             return issue->info().is_blocking;
                           });
    if (it == issues_.end())
      it = issues_.begin();

    new_top_issue = it->get();
  }

  // If we've found a new top issue, then report it via the observer.
  if (new_top_issue != top_issue_) {
    top_issue_ = new_top_issue;
    for (auto& observer : issues_observers_) {
      if (top_issue_)
        observer.OnIssue(*top_issue_);
      else
        observer.OnIssuesCleared();
    }
  }
}

}  // namespace media_router
