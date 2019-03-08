// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LARGEST_CONTENTFUL_PAINT_HANDLER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LARGEST_CONTENTFUL_PAINT_HANDLER_H_

#include <map>

#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"

namespace page_load_metrics {

class TimingInfo {
 public:
  explicit TimingInfo(
      page_load_metrics::PageLoadMetricsObserver::LargestContentType);
  explicit TimingInfo(
      const base::Optional<base::TimeDelta>&,
      const uint64_t& size,
      const page_load_metrics::PageLoadMetricsObserver::LargestContentType);
  explicit TimingInfo(const TimingInfo& other);
  void Reset(const base::Optional<base::TimeDelta>&, const uint64_t& size);
  base::Optional<base::TimeDelta> Time() const {
    DCHECK(HasConsistentTimeAndSize());
    return time_;
  }
  uint64_t Size() const {
    DCHECK(HasConsistentTimeAndSize());
    return size_;
  }
  page_load_metrics::PageLoadMetricsObserver::LargestContentType Type() const {
    DCHECK(HasConsistentTimeAndSize());
    return type_;
  }
  bool IsEmpty() const {
    DCHECK(HasConsistentTimeAndSize());
    // |size_| will be 0 as well, as checked by the DCHECK.
    return !time_;
  }

 private:
  TimingInfo() = delete;
  // This is only for DCHECK. We will never need the inconsistent state.
  bool HasConsistentTimeAndSize() const {
    return (time_ && size_) || (!time_ && !size_);
  }
  base::Optional<base::TimeDelta> time_;
  uint64_t size_;
  page_load_metrics::PageLoadMetricsObserver::LargestContentType type_;
};

class ContentfulPaint {
 public:
  ContentfulPaint();
  TimingInfo& Text() { return text_; }
  TimingInfo& Image() { return image_; }
  const TimingInfo& MergeTextAndImageTiming();

 private:
  TimingInfo text_;
  TimingInfo image_;
};

class LargestContentfulPaintHandler {
 public:
  static void SetTestMode(bool enabled);
  LargestContentfulPaintHandler();
  ~LargestContentfulPaintHandler();
  using FrameTreeNodeId =
      page_load_metrics::PageLoadMetricsObserver::FrameTreeNodeId;
  void RecordTiming(const page_load_metrics::mojom::PaintTimingPtr&,
                    content::RenderFrameHost* subframe_rfh);
  // We merge the candidates from main frame and subframe to get the largest
  // candidate across all frames.
  const TimingInfo& MergeMainFrameAndSubframes();
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle,
      const page_load_metrics::PageLoadExtraInfo& extra_info);

 private:
  void RecordSubframeTiming(const mojom::PaintTimingPtr& timing,
                            const base::TimeDelta& navigation_start_offset);
  void RecordMainFrameTiming(const page_load_metrics::mojom::PaintTimingPtr&);
  ContentfulPaint main_frame_contentful_paint_;
  ContentfulPaint subframe_contentful_paint_;

  // Navigation start offsets for the most recently committed document in each
  // frame.
  std::map<FrameTreeNodeId, base::TimeDelta> subframe_navigation_start_offset_;
  DISALLOW_COPY_AND_ASSIGN(LargestContentfulPaintHandler);
};

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LARGEST_CONTENTFUL_PAINT_HANDLER_H_
