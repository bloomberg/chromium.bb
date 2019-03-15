// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_

#include "base/optional.h"
#include "base/time/time.h"

namespace content {
class WebContents;
}  // namespace content

// This class tracks global state for the page load that should be accessible
// from any PageLoadMetricsObserver.
class PageLoadMetricsObserverDelegate {
 public:
  virtual content::WebContents* GetWebContents() const = 0;
  virtual base::TimeTicks GetNavigationStart() const = 0;
  virtual bool DidCommit() const = 0;

  // Get the amount of time the page has been in the foreground.
  virtual base::TimeDelta GetForegroundDuration() const = 0;

  // TODO(crbug/939403): Consider migrating PageLoadExtraInfo data to this API
  // and deprecating that struct.
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_
