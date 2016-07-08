// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/issue_manager.h"

#include <algorithm>

#include "content/public/browser/browser_thread.h"

namespace media_router {

IssueManager::IssueManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

IssueManager::~IssueManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void IssueManager::AddIssue(const Issue& issue) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const Issue& next_issue : issues_) {
    if (next_issue.Equals(issue)) {
      return;
    }
  }
  issues_.push_back(issue);
  MaybeUpdateTopIssue();
}

void IssueManager::ClearIssue(const Issue::Id& issue_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issues_.erase(std::remove_if(issues_.begin(), issues_.end(),
                               [&issue_id](const Issue& issue) {
                                 return issue_id == issue.id();
                               }),
                issues_.end());
  MaybeUpdateTopIssue();
}

size_t IssueManager::GetIssueCount() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return issues_.size();
}

void IssueManager::ClearAllIssues() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issues_.clear();
  MaybeUpdateTopIssue();
}

void IssueManager::ClearGlobalIssues() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issues_.erase(
      std::remove_if(issues_.begin(), issues_.end(), [](const Issue& issue) {
        return issue.is_global();
      }), issues_.end());
  MaybeUpdateTopIssue();
}

void IssueManager::ClearIssuesWithRouteId(const MediaRoute::Id& route_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issues_.erase(std::remove_if(issues_.begin(), issues_.end(),
                               [&route_id](const Issue& issue) {
                                 return route_id == issue.route_id();
                               }),
                issues_.end());
  MaybeUpdateTopIssue();
}

void IssueManager::RegisterObserver(IssuesObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  DCHECK(!issues_observers_.HasObserver(observer));

  issues_observers_.AddObserver(observer);
  if (top_issue_id_.empty())
    return;

  // Find the current top issue and report it to the observer.
  for (const auto& next_issue : issues_) {
    if (next_issue.id() == top_issue_id_) {
      observer->OnIssueUpdated(&next_issue);
    }
  }
}

void IssueManager::UnregisterObserver(IssuesObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issues_observers_.RemoveObserver(observer);
}

void IssueManager::MaybeUpdateTopIssue() {
  const Issue* new_top_issue = nullptr;

  if (issues_.empty()) {
    FOR_EACH_OBSERVER(IssuesObserver, issues_observers_,
                      OnIssueUpdated(new_top_issue));
    return;
  }

  // Select the first blocking issue in the list of issues.
  // If there are none, simply select the first issue in the list.
  new_top_issue = &(issues_.front());
  for (const auto& issue : issues_) {
    // The first blocking issue is of higher priority than the first issue.
    if (issue.is_blocking()) {
      new_top_issue = &issue;
      break;
    }
  }

  // If we've found a new top issue, then report it via the observer.
  if (new_top_issue->id() != top_issue_id_) {
    top_issue_id_ = new_top_issue->id();
    FOR_EACH_OBSERVER(IssuesObserver, issues_observers_,
                      OnIssueUpdated(new_top_issue));
  }
}

}  // namespace media_router
