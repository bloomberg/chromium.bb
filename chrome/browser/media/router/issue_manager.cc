// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/issue_manager.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"

namespace media_router {

namespace {

// The number of minutes a NOTIFICATION Issue stays in the IssueManager
// before it is auto-dismissed.
constexpr int kNotificationAutoDismissMins = 1;

// The number of minutes a WARNING Issue stays in the IssueManager before it
// is auto-dismissed.
constexpr int kWarningAutoDismissMins = 5;

}  // namespace

// static
base::TimeDelta IssueManager::GetAutoDismissTimeout(
    const IssueInfo& issue_info) {
  if (issue_info.is_blocking)
    return base::TimeDelta();

  switch (issue_info.severity) {
    case IssueInfo::Severity::NOTIFICATION:
      return base::TimeDelta::FromMinutes(kNotificationAutoDismissMins);
    case IssueInfo::Severity::WARNING:
      return base::TimeDelta::FromMinutes(kWarningAutoDismissMins);
    case IssueInfo::Severity::FATAL:
      NOTREACHED() << "FATAL issues should be blocking";
      return base::TimeDelta();
  }
  NOTREACHED();
  return base::TimeDelta();
}

IssueManager::IssueManager()
    : top_issue_(nullptr),
      task_runner_(content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::UI)) {}

IssueManager::~IssueManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IssueManager::AddIssue(const IssueInfo& issue_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = std::find_if(issues_.begin(), issues_.end(),
                         [&issue_info](const std::unique_ptr<Issue>& issue) {
                           return issue_info == issue->info();
                         });
  if (it != issues_.end())
    return;

  auto issue = base::MakeUnique<Issue>(issue_info);
  base::TimeDelta timeout = GetAutoDismissTimeout(issue_info);
  if (!timeout.is_zero()) {
    const Issue::Id& issue_id = issue->id();
    auto auto_dismiss_issue_cb = base::Bind(
        &IssueManager::ClearIssue, base::Unretained(this), issue->id());
    auto_dismiss_issue_callbacks_.emplace(
        issue_id,
        base::MakeUnique<base::CancelableClosure>(auto_dismiss_issue_cb));
    task_runner_->PostDelayedTask(FROM_HERE, auto_dismiss_issue_cb, timeout);
  }

  issues_.push_back(std::move(issue));
  MaybeUpdateTopIssue();
}

void IssueManager::ClearIssue(const Issue::Id& issue_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  issues_.erase(
      std::remove_if(issues_.begin(), issues_.end(),
                     [&issue_id](const std::unique_ptr<Issue>& issue) {
                       return issue_id == issue->id();
                     }),
      issues_.end());
  auto_dismiss_issue_callbacks_.erase(issue_id);
  MaybeUpdateTopIssue();
}

void IssueManager::RegisterObserver(IssuesObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  DCHECK(!issues_observers_.HasObserver(observer));

  issues_observers_.AddObserver(observer);
  MaybeUpdateTopIssue();
  if (top_issue_)
    observer->OnIssue(*top_issue_);
}

void IssueManager::UnregisterObserver(IssuesObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
