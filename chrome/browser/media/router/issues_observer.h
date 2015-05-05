// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_ISSUES_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_ISSUES_OBSERVER_H_

#include "chrome/browser/media/router/issue.h"

namespace media_router {

// Interface for observing when issues have been updated.
// TODO(imcheng): Implement automatic register/unregister from MediaRouter.
class IssuesObserver {
 public:
  virtual ~IssuesObserver() {}

  // Called when there is a new highest priority issue.
  // |issue| can be nullptr.
  virtual void OnIssueUpdated(const Issue* issue) = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_ISSUES_OBSERVER_H_
