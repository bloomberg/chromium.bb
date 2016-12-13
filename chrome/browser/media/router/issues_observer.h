// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_ISSUES_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_ISSUES_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/media/router/issue.h"

namespace media_router {

class MediaRouter;

// Base class for observing Media Router Issue. There is at most one Issue
// at any given time.
class IssuesObserver {
 public:
  explicit IssuesObserver(MediaRouter* router);
  virtual ~IssuesObserver();

  // Registers with Media Router to start observing for Issues. No-ops if Init()
  // has already been called before.
  void Init();

  // Called when there is an updated Media Router Issue.
  // Note that |issue| is owned by the IssueManager that is calling the
  // observers. Implementations that wish to retain the data must make a copy
  // of |issue|.
  virtual void OnIssue(const Issue& issue) {}

  // Called when there are no more issues.
  virtual void OnIssuesCleared() {}

 private:
  MediaRouter* router_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(IssuesObserver);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_ISSUES_OBSERVER_H_
