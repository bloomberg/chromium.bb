// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_activity_watcher.h"

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_metrics_logger_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/window_activity_watcher.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(TabActivityWatcher::WebContentsData);

namespace {

// Default time before a tab with the same SourceId can be logged again.
constexpr base::TimeDelta kDefaultPerSourceLogTimeout =
    base::TimeDelta::FromSeconds(10);

// Fieldtrial param to override the default per-source log timeout above.
constexpr char kPerSourceLogTimeoutMsecParamName[] =
    "per_source_log_timeout_msec";

}  // namespace

// Per-WebContents helper class that observes its WebContents, notifying
// TabActivityWatcher when interesting events occur. Also provides
// per-WebContents data that TabActivityWatcher uses to log the tab.
class TabActivityWatcher::WebContentsData
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebContentsData>,
      public content::RenderWidgetHost::InputEventObserver {
 public:
  ~WebContentsData() override = default;

  // Call after logging to update |last_log_time_for_source_|.
  void DidLog(base::TimeTicks log_time) {
    last_log_time_for_source_ = log_time;
  }

  ukm::SourceId ukm_source_id() const { return ukm_source_id_; }

  const TabMetricsLogger::TabMetrics& tab_metrics() const {
    return tab_metrics_;
  }

  base::TimeTicks last_log_time_for_source() const {
    return last_log_time_for_source_;
  }

 private:
  friend class content::WebContentsUserData<WebContentsData>;

  explicit WebContentsData(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    tab_metrics_.web_contents = web_contents;
    web_contents->GetRenderViewHost()->GetWidget()->AddInputEventObserver(this);
  }

  // content::WebContentsObserver:
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override {
    if (old_host != nullptr)
      old_host->GetWidget()->RemoveInputEventObserver(this);
    new_host->GetWidget()->AddInputEventObserver(this);
  }
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted() ||
        !navigation_handle->IsInMainFrame() ||
        navigation_handle->IsSameDocument()) {
      return;
    }

    // Use the same SourceId that SourceUrlRecorderWebContentsObserver populates
    // and updates.
    ukm::SourceId new_source_id = ukm::ConvertToSourceId(
        navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
    DCHECK_NE(new_source_id, ukm_source_id_)
        << "Expected a unique Source ID for the navigation";
    ukm_source_id_ = new_source_id;

    // Clear the per-SourceId last log time.
    last_log_time_for_source_ = base::TimeTicks();

    // Reset the per-page data.
    tab_metrics_.page_metrics = {};
  }
  void DidStopLoading() override {
    // Log metrics for the tab when it stops loading instead of immediately
    // after a navigation commits, so we can have some idea of its status and
    // contents.
    TabActivityWatcher::GetInstance()->OnDidStopLoading(web_contents());
  }
  void WasHidden() override {
    TabActivityWatcher::GetInstance()->OnWasHidden(web_contents());
  }

  // content::RenderWidgetHost::InputEventObserver:
  void OnInputEvent(const blink::WebInputEvent& event) override {
    if (blink::WebInputEvent::IsMouseEventType(event.GetType()))
      tab_metrics_.page_metrics.mouse_event_count++;
    else if (blink::WebInputEvent::IsKeyboardEventType(event.GetType()))
      tab_metrics_.page_metrics.key_event_count++;
    else if (blink::WebInputEvent::IsTouchEventType(event.GetType()))
      tab_metrics_.page_metrics.touch_event_count++;
  }

  // Updated when a navigation is finished.
  ukm::SourceId ukm_source_id_ = 0;

  // Used to throttle event logging per SourceId.
  base::TimeTicks last_log_time_for_source_;

  // Stores current stats for the tab.
  TabMetricsLogger::TabMetrics tab_metrics_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsData);
};

TabActivityWatcher::TabActivityWatcher()
    : tab_metrics_logger_(std::make_unique<TabMetricsLoggerImpl>()),
      browser_tab_strip_tracker_(this, this, nullptr) {
  per_source_log_timeout_ =
      base::TimeDelta::FromMilliseconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kTabMetricsLogging, kPerSourceLogTimeoutMsecParamName,
          kDefaultPerSourceLogTimeout.InMilliseconds()));
  browser_tab_strip_tracker_.Init();

  // TabMetrics UKMs reference WindowMetrics UKM entries, so ensure the
  // WindowActivityWatcher is initialized.
  WindowActivityWatcher::GetInstance();
}

TabActivityWatcher::~TabActivityWatcher() = default;

void TabActivityWatcher::TabPinnedStateChanged(TabStripModel* tab_strip_model,
                                               content::WebContents* contents,
                                               int index) {
  // We only want UKMs for background tabs.
  if (tab_strip_model->active_index() == index)
    return;

  MaybeLogTab(contents);
}

bool TabActivityWatcher::ShouldTrackBrowser(Browser* browser) {
  // Don't track incognito browsers. This is also enforced by UKM.
  return !browser->profile()->IsOffTheRecord();
}

void TabActivityWatcher::OnWasHidden(content::WebContents* web_contents) {
  DCHECK(web_contents);

  MaybeLogTab(web_contents);
}

void TabActivityWatcher::OnDidStopLoading(content::WebContents* web_contents) {
  // Ignore load events in foreground tabs. The tab state of a foreground tab
  // will be logged if/when it is backgrounded.
  if (web_contents->IsVisible())
    return;
  MaybeLogTab(web_contents);
}

void TabActivityWatcher::MaybeLogTab(content::WebContents* web_contents) {
  // Don't log when the WebContents is being closed or replaced.
  if (web_contents->IsBeingDestroyed())
    return;

  DCHECK(!web_contents->GetBrowserContext()->IsOffTheRecord());

  TabActivityWatcher::WebContentsData* web_contents_data =
      TabActivityWatcher::WebContentsData::FromWebContents(web_contents);
  DCHECK(web_contents_data);

  base::TimeTicks now = base::TimeTicks::Now();
  if (now - web_contents_data->last_log_time_for_source() <
      per_source_log_timeout_) {
    return;
  }

  ukm::SourceId ukm_source_id = web_contents_data->ukm_source_id();
  tab_metrics_logger_->LogBackgroundTab(ukm_source_id,
                                        web_contents_data->tab_metrics());
  web_contents_data->DidLog(now);
}

void TabActivityWatcher::DisableLogTimeoutForTesting() {
  per_source_log_timeout_ = base::TimeDelta();
}

void TabActivityWatcher::ResetForTesting() {
  tab_metrics_logger_ = std::make_unique<TabMetricsLoggerImpl>();
}

// static
TabActivityWatcher* TabActivityWatcher::GetInstance() {
  CR_DEFINE_STATIC_LOCAL(TabActivityWatcher, instance, ());
  return &instance;
}

// static
void TabActivityWatcher::WatchWebContents(content::WebContents* web_contents) {
  // In incognito, the UKM service won't log anything.
  if (!web_contents->GetBrowserContext()->IsOffTheRecord())
    TabActivityWatcher::WebContentsData::CreateForWebContents(web_contents);
}
