// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_
#define CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

class ChromeBrowserMainParts;
class ChromeBrowserMetricsServiceObserver;

namespace chrome {
void AddMetricsExtraParts(ChromeBrowserMainParts* main_parts);
}

class ChromeBrowserMainExtraPartsMetrics : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsMetrics();
  virtual ~ChromeBrowserMainExtraPartsMetrics();

  // Overridden from ChromeBrowserMainExtraParts:
  virtual void PreProfileInit() OVERRIDE;
  virtual void PreBrowserStart() OVERRIDE;
  virtual void PostBrowserStart() OVERRIDE;

 private:
  // Observe and log histograms on new metric logs.
  scoped_ptr<ChromeBrowserMetricsServiceObserver> metrics_service_observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsMetrics);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_
