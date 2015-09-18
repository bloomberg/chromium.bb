// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_REVISIT_PAGE_VISIT_OBSERVERE_H_
#define COMPONENTS_SYNC_DRIVER_REVISIT_PAGE_VISIT_OBSERVERE_H_

#include "url/gurl.h"

namespace sync_driver {

// An interface that allows observers to be notified when a page is visited.
class PageVisitObserver {
 public:
  // This enum represents the most common ways to visit a new page/URL.
  enum TransitionType {
    kTransitionPage = 0,
    kTransitionOmniboxUrl = 1,
    kTransitionOmniboxDefaultSearch = 2,
    kTransitionOmniboxTemplateSearch = 3,
    kTransitionBookmark = 4,
    kTransitionCopyPaste = 5,
    kTransitionForwardBackward = 6,
    kTransitionRestore = 7,
    kTransitionUnknown = 8,
    kTransitionTypeLast = 9,
  };

  virtual ~PageVisitObserver() {}
  virtual void OnPageVisit(const GURL& url,
                           const TransitionType transition) = 0;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_REVISIT_PAGE_VISIT_OBSERVERE_H_
