// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_ISSUES_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_ISSUES_OBSERVER_H_

#include "chrome/browser/media/router/issue.h"

namespace media_router {

class MediaRouter;

// Base class for observing Media Router Issue. There is at most one Issue
// at any given time.
class IssuesObserver {
 public:
  explicit IssuesObserver(MediaRouter* router);
  virtual ~IssuesObserver();

  void RegisterObserver();
  void UnregisterObserver();

  // Called when there is an updated Media Router Issue.
  // If |issue| is nullptr, then there is currently no issue.
  virtual void OnIssueUpdated(const Issue* issue) {}

 private:
  MediaRouter* router_;

  DISALLOW_COPY_AND_ASSIGN(IssuesObserver);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_ISSUES_OBSERVER_H_
