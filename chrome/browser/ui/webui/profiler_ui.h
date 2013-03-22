// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PROFILER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PROFILER_UI_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/metrics/tracking_synchronizer_observer.h"
#include "content/public/browser/web_ui_controller.h"

// The C++ back-end for the chrome://profiler webui page.
class ProfilerUI : public content::WebUIController,
                   public chrome_browser_metrics::TrackingSynchronizerObserver {
 public:
  explicit ProfilerUI(content::WebUI* web_ui);
  virtual ~ProfilerUI();

  // Get the tracking data from TrackingSynchronizer.
  void GetData();

 private:
  // TrackingSynchronizerObserver:
  virtual void ReceivedProfilerData(
      const tracked_objects::ProcessDataSnapshot& profiler_data,
      int process_type) OVERRIDE;

  // Used to get |weak_ptr_| to self on the UI thread.
  base::WeakPtrFactory<ProfilerUI> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfilerUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PROFILER_UI_H_
