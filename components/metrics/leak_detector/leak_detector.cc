// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/leak_detector.h"

#include "content/public/browser/browser_thread.h"

namespace metrics {

LeakDetector::LeakReport::LeakReport() {}

LeakDetector::LeakReport::~LeakReport() {}

LeakDetector::LeakDetector(float sampling_rate,
                           size_t max_call_stack_unwind_depth,
                           uint64_t analysis_interval_bytes,
                           uint32_t size_suspicion_threshold,
                           uint32_t call_stack_suspicion_threshold)
    : weak_factory_(this) {
  // TODO(sque): Connect this class to LeakDetectorImpl and base::allocator.
}

LeakDetector::~LeakDetector() {}

void LeakDetector::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void LeakDetector::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void LeakDetector::NotifyObservers(const std::vector<LeakReport>& reports) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&LeakDetector::NotifyObservers, weak_factory_.GetWeakPtr(),
                   reports));
    return;
  }

  for (const LeakReport& report : reports) {
    FOR_EACH_OBSERVER(Observer, observers_, OnLeakFound(report));
  }
}

}  // namespace metrics
