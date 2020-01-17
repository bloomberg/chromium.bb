// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/first_web_contents_profiler.h"

#include <string>

#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {

// Reasons for which profiling is deemed complete. Logged in UMA (do not re-
// order or re-assign).
enum class FinishReason {
  // All metrics were successfully gathered.
  kDone = 0,
  // Abandon if blocking UI was shown during startup.
  kAbandonBlockingUI = 1,
  // Abandon if the WebContents is hidden (lowers scheduling priority).
  kAbandonContentHidden = 2,
  // Abandon if the WebContents is destroyed.
  kAbandonContentDestroyed = 3,
  // Abandon if the WebContents navigates away from its initial page, as it:
  //   (1) is no longer a fair timing; and
  //   (2) can cause http://crbug.com/525209 where the first paint didn't fire
  //       for the initial content but fires after a lot of idle time when the
  //       user finally navigates to another page that does trigger it.
  kAbandonNewNavigation = 4,
  // Abandon if the WebContents fails to load (e.g. network error, etc.).
  kAbandonNavigationError = 5,
  // Abandon if no WebContents was visible at the beginning of startup
  kAbandonNoInitiallyVisibleContent = 6,
  kMaxValue = kAbandonNoInitiallyVisibleContent
};

// Per documentation in navigation_request.cc, a navigation id is guaranteed
// nonzero.
constexpr int64_t kInvalidNavigationId = 0;

void RecordFinishReason(FinishReason finish_reason) {
  base::UmaHistogramEnumeration("Startup.FirstWebContents.FinishReason",
                                finish_reason);
}

class FirstWebContentsProfiler : public content::WebContentsObserver {
 public:
  FirstWebContentsProfiler(content::WebContents* web_contents,
                           startup_metric_utils::WebContentsWorkload workload);

 private:
  ~FirstWebContentsProfiler() override = default;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

  // Logs |finish_reason| to UMA and deletes this FirstWebContentsProfiler.
  void FinishedCollectingMetrics(FinishReason finish_reason);

  const startup_metric_utils::WebContentsWorkload workload_;

  // The first NavigationHandle id observed by this.
  int64_t first_navigation_id_ = kInvalidNavigationId;

  // Whether a main frame navigation finished since this was created.
  bool did_finish_first_navigation_ = false;

  DISALLOW_COPY_AND_ASSIGN(FirstWebContentsProfiler);
};

FirstWebContentsProfiler::FirstWebContentsProfiler(
    content::WebContents* web_contents,
    startup_metric_utils::WebContentsWorkload workload)
    : content::WebContentsObserver(web_contents), workload_(workload) {}

void FirstWebContentsProfiler::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore subframe navigations and same-document navigations.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (first_navigation_id_ != kInvalidNavigationId) {
    // Abandon if this is not the first observed top-level navigation.
    DCHECK_NE(first_navigation_id_, navigation_handle->GetNavigationId());
    FinishedCollectingMetrics(FinishReason::kAbandonNewNavigation);
    return;
  }

  DCHECK(!did_finish_first_navigation_);

  // Keep track of the first top-level navigation id observed by this.
  first_navigation_id_ = navigation_handle->GetNavigationId();
}

void FirstWebContentsProfiler::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (startup_metric_utils::WasMainWindowStartupInterrupted()) {
    FinishedCollectingMetrics(FinishReason::kAbandonBlockingUI);
    return;
  }

  // Ignore subframe navigations and same-document navigations.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsErrorPage()) {
    FinishedCollectingMetrics(FinishReason::kAbandonNavigationError);
    return;
  }

  if (first_navigation_id_ == kInvalidNavigationId) {
    // Keep track of the first top-level navigation id observed by this.
    //
    // Note: FirstWebContentsProfiler may be created before or after
    // DidStartNavigation() is dispatched for the first navigation, which is why
    // |first_navigation_id_| may be set in DidStartNavigation() or
    // DidFinishNavigation().
    first_navigation_id_ = navigation_handle->GetNavigationId();
  }

  DCHECK_EQ(first_navigation_id_, navigation_handle->GetNavigationId());
  DCHECK(!did_finish_first_navigation_);
  did_finish_first_navigation_ = true;

  startup_metric_utils::RecordFirstWebContentsMainNavigationStart(
      navigation_handle->NavigationStart(), workload_);
  startup_metric_utils::RecordFirstWebContentsMainNavigationFinished(
      base::TimeTicks::Now());
}

void FirstWebContentsProfiler::DidFirstVisuallyNonEmptyPaint() {
  DCHECK(did_finish_first_navigation_);

  if (startup_metric_utils::WasMainWindowStartupInterrupted()) {
    FinishedCollectingMetrics(FinishReason::kAbandonBlockingUI);
    return;
  }

  startup_metric_utils::RecordFirstWebContentsNonEmptyPaint(
      base::TimeTicks::Now(), web_contents()
                                  ->GetMainFrame()
                                  ->GetProcess()
                                  ->GetInitTimeForNavigationMetrics());

  FinishedCollectingMetrics(FinishReason::kDone);
}

void FirstWebContentsProfiler::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility != content::Visibility::VISIBLE) {
    // Stop profiling if the content gets hidden as its load may be
    // deprioritized and timing it becomes meaningless.
    FinishedCollectingMetrics(FinishReason::kAbandonContentHidden);
  }
}

void FirstWebContentsProfiler::WebContentsDestroyed() {
  FinishedCollectingMetrics(FinishReason::kAbandonContentDestroyed);
}

void FirstWebContentsProfiler::FinishedCollectingMetrics(
    FinishReason finish_reason) {
  RecordFinishReason(finish_reason);
  delete this;
}

}  // namespace

namespace metrics {

void BeginFirstWebContentsProfiling() {
  using startup_metric_utils::WebContentsWorkload;

  const BrowserList* browser_list = BrowserList::GetInstance();

  content::WebContents* visible_contents = nullptr;
  for (Browser* browser : *browser_list) {
    if (!browser->window()->IsVisible())
      continue;

    // The active WebContents may be hidden when the window height is small.
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();

#if defined(OS_MACOSX)
    // TODO(https://crbug.com/1032348): It is incorrect to have a visible
    // browser window with no active WebContents, but reports on Mac show that
    // it happens.
    if (!contents)
      continue;
#endif  // defined(OS_MACOSX)

    if (contents->GetVisibility() != content::Visibility::VISIBLE)
      continue;

    visible_contents = contents;
    break;
  }

  if (!visible_contents) {
    RecordFinishReason(FinishReason::kAbandonNoInitiallyVisibleContent);
    return;
  }

  const bool single_tab = browser_list->size() == 1 &&
                          browser_list->get(0)->tab_strip_model()->count() == 1;

  // FirstWebContentsProfiler owns itself and is also bound to
  // |visible_contents|'s lifetime by observing WebContentsDestroyed().
  new FirstWebContentsProfiler(visible_contents,
                               single_tab ? WebContentsWorkload::SINGLE_TAB
                                          : WebContentsWorkload::MULTI_TABS);
}

}  // namespace metrics
