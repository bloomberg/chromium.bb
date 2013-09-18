// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SWAP_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_SWAP_METRICS_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/events/event_handler.h"

namespace base {
class FilePath;
}

namespace chromeos {

// Watches for bursts of swap activity and CPU consumption after interesting UI
// events like tab switch or scrolling, recording the values to UMA statistics.
// Only records stats for the last active browser.
class SwapMetrics : public chrome::BrowserListObserver,
                    public TabStripModelObserver,
                    public ui::EventHandler {
 public:
  SwapMetrics();
  virtual ~SwapMetrics();

  // chrome::BrowserListObserver overrides:
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;
  virtual void OnBrowserSetLastActive(Browser* browser) OVERRIDE;

  // TabStripModelObserver overrides:
  virtual void ActiveTabChanged(content::WebContents* old_contents,
                                content::WebContents* new_contents,
                                int index,
                                int reason) OVERRIDE;

  // ui::EventHandler overrides:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;

 private:
  class Backend;

  // Posts a task to record metrics for |sample_index| after |delay_ms|.
  static void PostTaskRecordMetrics(scoped_refptr<Backend> backend,
                                    size_t sample_index,
                                    int delay_ms);

  // Starts a metrics collection run, canceling any run already in progress.
  void StartMetricsCollection(const std::string& reason);

  // Sets the browser being monitored for events.
  void SetBrowser(Browser* browser);

  // Browser being monitored for UI events.
  Browser* browser_;

  // Backend to handle processing in the blocking thread pool.
  scoped_refptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(SwapMetrics);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SWAP_METRICS_H_
