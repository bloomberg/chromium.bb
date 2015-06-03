// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_ISSUE_MANAGER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_ISSUE_MANAGER_H_

#include <vector>

#include "base/containers/hash_tables.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/issues_observer.h"

namespace media_router {

// IssueManager keeps track of current issues related to casting
// connectivity and quality.
// TODO(apacible): Determine what other issues will be handled here.
class IssueManager {
 public:
  IssueManager();
  ~IssueManager();

  // Adds an issue.
  // |issue|: Issue to be added. Must have unique ID.
  void AddIssue(const Issue& issue);

  // Removes an issue when user has noted it is resolved.
  // |issue_id|: Issue::Id of the issue to be removed.
  void ClearIssue(const Issue::Id& issue_id);

  // Gets the number of unresolved issues.
  size_t GetIssueCount() const;

  // Removes all unresolved issues.
  void ClearAllIssues();

  // Removes all unresolved global issues.
  void ClearGlobalIssues();

  // Removes all unresolved issues with RouteId.
  // |route_id|: ID of the media route whose issues are to be cleared.
  void ClearIssuesWithRouteId(const MediaRoute::Id& route_id);

  // Registers an issue observer |observer|. The observer will be triggered
  // when the highest priority issue changes.
  // If there is already an observer registered with this instance, do nothing.
  // Does not assume ownership of |observer|.
  // |observer|: IssuesObserver to be registered.
  void RegisterObserver(IssuesObserver* observer);

  // Unregisters |observer| from |issues_observers_|.
  // |observer|: IssuesObserver to be unregistered.
  void UnregisterObserver(IssuesObserver* observer);

 private:
  // Checks if the current top issue has changed. Updates |top_issue_|.
  // If |top_issue_| has changed, issues in |issues_observers_| will be
  // notified of the new top issue.
  void MaybeUpdateTopIssue();

  std::vector<Issue> issues_;

  // IssueObserver insteances are not owned by the manager.
  base::ObserverList<IssuesObserver> issues_observers_;

  // The ID of the current top issue.
  Issue::Id top_issue_id_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(IssueManager);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_ISSUE_MANAGER_H_
