// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_ISSUE_MANAGER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_ISSUE_MANAGER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/issues_observer.h"

namespace media_router {

// IssueManager keeps track of current issues related to casting
// connectivity and quality. It lives on the UI thread.
// TODO(apacible): Determine what other issues will be handled here.
class IssueManager {
 public:
  IssueManager();
  ~IssueManager();

  // Adds an issue. No-ops if the issue already exists.
  // |issue_info|: Info of issue to be added.
  void AddIssue(const IssueInfo& issue_info);

  // Removes an issue when user has noted it is resolved.
  // |issue_id|: Issue::Id of the issue to be removed.
  void ClearIssue(const Issue::Id& issue_id);

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
  // If |top_issue_| has changed, observers in |issues_observers_| will be
  // notified of the new top issue.
  void MaybeUpdateTopIssue();

  std::vector<std::unique_ptr<Issue>> issues_;

  // IssueObserver insteances are not owned by the manager.
  base::ObserverList<IssuesObserver> issues_observers_;

  // ID of the top Issue in |issues_|, or |nullptr| if there are no issues.
  const Issue* top_issue_;

  DISALLOW_COPY_AND_ASSIGN(IssueManager);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_ISSUE_MANAGER_H_
