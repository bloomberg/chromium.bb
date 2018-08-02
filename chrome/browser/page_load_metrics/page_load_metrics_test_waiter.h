// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_TEST_WAITER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_TEST_WAITER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "content/public/browser/render_frame_host.h"

namespace page_load_metrics {

class PageLoadMetricsTestWaiter
    : public page_load_metrics::MetricsWebContentsObserver::TestingObserver {
 public:
  // A bitvector to express which timing fields to match on.
  enum class TimingField : int {
    kFirstLayout = 1 << 0,
    kFirstPaint = 1 << 1,
    kFirstContentfulPaint = 1 << 2,
    kFirstMeaningfulPaint = 1 << 3,
    kDocumentWriteBlockReload = 1 << 4,
    kLoadEvent = 1 << 5,
    // kLoadTimingInfo waits for main frame timing info only.
    kLoadTimingInfo = 1 << 6,
  };

  explicit PageLoadMetricsTestWaiter(content::WebContents* web_contents);

  ~PageLoadMetricsTestWaiter() override;

  // Add a page-level expectation.
  void AddPageExpectation(TimingField field);

  // Add a subframe-level expectation.
  void AddSubFrameExpectation(TimingField field);

  // Add a data use expectation
  void AddMinimumPageLoadDataUseExpectation(
      int expected_minimum_page_load_data_use);

  // Whether the given TimingField was observed in the page.
  bool DidObserveInPage(TimingField field) const;

  // Waits for PageLoadMetrics events that match the fields set by
  // |AddPageExpectation| and |AddSubFrameExpectation|. All matching fields
  // must be set to end this wait.
  void Wait();

  int64_t current_page_load_data_use() const {
    return current_page_load_data_use_;
  }

 private:
  // PageLoadMetricsObserver used by the PageLoadMetricsTestWaiter to observe
  // metrics updates.
  class WaiterMetricsObserver
      : public page_load_metrics::PageLoadMetricsObserver {
   public:
    // We use a WeakPtr to the PageLoadMetricsTestWaiter because |waiter| can be
    // destroyed before this WaiterMetricsObserver.
    explicit WaiterMetricsObserver(
        base::WeakPtr<PageLoadMetricsTestWaiter> waiter);
    ~WaiterMetricsObserver() override;

    void OnTimingUpdate(
        content::RenderFrameHost* subframe_rfh,
        const page_load_metrics::mojom::PageLoadTiming& timing,
        const page_load_metrics::PageLoadExtraInfo& extra_info) override;

    void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                              extra_request_complete_info) override;

    void OnDataUseObserved(int64_t received_data_length,
                           int64_t data_reduction_proxy_bytes_saved) override;

   private:
    const base::WeakPtr<PageLoadMetricsTestWaiter> waiter_;
  };

  // Manages a bitset of TimingFields.
  class TimingFieldBitSet {
   public:
    TimingFieldBitSet() {}

    // Returns whether this bitset has all bits unset.
    bool Empty() const { return bitmask_ == 0; }

    // Returns whether this bitset has the given bit set.
    bool IsSet(TimingField field) const {
      return (bitmask_ & static_cast<int>(field)) != 0;
    }

    // Sets the bit for the given |field|.
    void Set(TimingField field) { bitmask_ |= static_cast<int>(field); }

    // Clears the bit for the given |field|.
    void Clear(TimingField field) { bitmask_ &= ~static_cast<int>(field); }

    // Merges bits set in |other| into this bitset.
    void Merge(const TimingFieldBitSet& other) { bitmask_ |= other.bitmask_; }

    // Clears all bits set in the |other| bitset.
    void ClearMatching(const TimingFieldBitSet& other) {
      bitmask_ &= ~other.bitmask_;
    }

   private:
    int bitmask_ = 0;
  };

  static bool IsPageLevelField(TimingField field);

  static TimingFieldBitSet GetMatchedBits(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::mojom::PageLoadMetadata& metadata);

  // Updates observed page fields when a timing update is received by the
  // MetricsWebContentsObserver. Stops waiting if expectations are satsfied
  // after update.
  void OnTimingUpdated(content::RenderFrameHost* subframe_rfh,
                       const page_load_metrics::mojom::PageLoadTiming& timing,
                       const page_load_metrics::PageLoadExtraInfo& extra_info);

  // Updates observed page fields when a resource load is observed by
  // MetricsWebContentsObserver.  Stops waiting if expectations are satsfied
  // after update.
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_complete_info);

  // Updates data counters when data use is seen by the
  // MetricsWebContentsObserver. Stops waiting if expectations are satsfied
  // after update.
  void OnDataUseObserved(int64_t received_data_length,
                         int64_t data_reduction_proxy_bytes_saved);

  void OnTrackerCreated(page_load_metrics::PageLoadTracker* tracker) override;

  void OnCommit(page_load_metrics::PageLoadTracker* tracker) override;

  bool expectations_satisfied() const;

  std::unique_ptr<base::RunLoop> run_loop_;

  TimingFieldBitSet page_expected_fields_;
  TimingFieldBitSet subframe_expected_fields_;

  TimingFieldBitSet observed_page_fields_;

  int64_t expected_minimum_page_load_data_use_ = 0;
  int64_t current_page_load_data_use_ = 0;

  bool attach_on_tracker_creation_ = false;
  bool did_add_observer_ = false;

  base::WeakPtrFactory<PageLoadMetricsTestWaiter> weak_factory_;
};

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_TEST_WAITER_H_
