// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_
#define CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/metrics/first_web_contents_profiler.h"
#include "ui/gfx/display_observer.h"

class ChromeBrowserMainParts;

namespace chrome {
void AddMetricsExtraParts(ChromeBrowserMainParts* main_parts);
}

class ChromeBrowserMainExtraPartsMetrics
    : public ChromeBrowserMainExtraParts,
      public gfx::DisplayObserver,
      public FirstWebContentsProfiler::Delegate {
 public:
  ChromeBrowserMainExtraPartsMetrics();
  ~ChromeBrowserMainExtraPartsMetrics() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PreProfileInit() override;
  void PreBrowserStart() override;
  void PostBrowserStart() override;

 private:
#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Records Mac specific metrics.
  void RecordMacMetrics();
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

  // DisplayObserver overrides.
  void OnDisplayAdded(const gfx::Display& new_display) override;
  void OnDisplayRemoved(const gfx::Display& old_display) override;
  void OnDisplayMetricsChanged(const gfx::Display& display,
                               uint32_t changed_metrics) override;

  // FirstWebContentsProfilerDelegate overrides.
  void ProfilerFinishedCollectingMetrics() override;

  // If the number of displays has changed, emit a UMA metric.
  void EmitDisplaysChangedMetric();

  // A cached value for the number of displays.
  int display_count_;

  // True iff |this| instance is registered as an observer of the native
  // screen.
  bool is_screen_observer_;

  // Measures start up performance of the first active web contents.
  scoped_ptr<FirstWebContentsProfiler> first_web_contents_profiler_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsMetrics);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_
