// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_metrics_logger_impl.h"

#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_metrics_event.pb.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_importance_signals.h"
#include "net/base/mime_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/WebKit/public/platform/WebSuddenTerminationDisablerType.h"

using metrics::TabMetricsEvent;

namespace {

// Returns the ContentType that matches |mime_type|.
TabMetricsEvent::ContentType GetContentTypeFromMimeType(
    const std::string& mime_type) {
  // Test for special cases before testing wildcard types.
  if (mime_type.empty())
    return TabMetricsEvent::CONTENT_TYPE_UNKNOWN;
  if (net::MatchesMimeType("text/html", mime_type))
    return TabMetricsEvent::CONTENT_TYPE_TEXT_HTML;
  if (net::MatchesMimeType("application/*", mime_type))
    return TabMetricsEvent::CONTENT_TYPE_APPLICATION;
  if (net::MatchesMimeType("audio/*", mime_type))
    return TabMetricsEvent::CONTENT_TYPE_AUDIO;
  if (net::MatchesMimeType("image/*", mime_type))
    return TabMetricsEvent::CONTENT_TYPE_IMAGE;
  if (net::MatchesMimeType("text/*", mime_type))
    return TabMetricsEvent::CONTENT_TYPE_TEXT;
  if (net::MatchesMimeType("video/*", mime_type))
    return TabMetricsEvent::CONTENT_TYPE_VIDEO;
  return TabMetricsEvent::CONTENT_TYPE_OTHER;
}

// Returns the site engagement score for the WebContents, rounded down to 10s
// to limit granularity.
int GetSiteEngagementScore(const content::WebContents* web_contents) {
  SiteEngagementService* service = SiteEngagementService::Get(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  DCHECK(service);

  // Scores range from 0 to 100. Round down to a multiple of 10 to conform to
  // privacy guidelines.
  double raw_score = service->GetScore(web_contents->GetVisibleURL());
  int rounded_score = static_cast<int>(raw_score / 10) * 10;
  DCHECK_LE(0, rounded_score);
  DCHECK_GE(100, rounded_score);
  return rounded_score;
}

}  // namespace

TabMetricsLoggerImpl::TabMetricsLoggerImpl() = default;
TabMetricsLoggerImpl::~TabMetricsLoggerImpl() = default;

void TabMetricsLoggerImpl::LogBackgroundTab(ukm::SourceId ukm_source_id,
                                            const TabMetrics& tab_metrics) {
  if (!ukm_source_id)
    return;

  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  content::WebContents* web_contents = tab_metrics.web_contents;
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  // UKM recording is disabled in OTR.
  if (web_contents->GetBrowserContext()->IsOffTheRecord())
    return;

  const TabStripModel* tab_strip_model = browser->tab_strip_model();
  int index = tab_strip_model->GetIndexOfWebContents(web_contents);
  DCHECK_NE(index, TabStripModel::kNoTab);

  ukm::builders::TabManager_TabMetrics entry(ukm_source_id);
  entry.SetKeyEventCount(tab_metrics.page_metrics.key_event_count)
      .SetMouseEventCount(tab_metrics.page_metrics.mouse_event_count)
      .SetTouchEventCount(tab_metrics.page_metrics.touch_event_count);

  if (SiteEngagementService::IsEnabled())
    entry.SetSiteEngagementScore(GetSiteEngagementScore(web_contents));

  TabMetricsEvent::ContentType content_type =
      GetContentTypeFromMimeType(web_contents->GetContentsMimeType());
  entry.SetContentType(static_cast<int>(content_type));
  // TODO(michaelpg): Add PluginType field if mime type matches "application/*"
  // using PluginUMAReporter.

  // This checks if the tab was audible within the past two seconds, same as the
  // audio indicator in the tab strip.
  entry.SetWasRecentlyAudible(web_contents->WasRecentlyAudible());

  resource_coordinator::TabManager* tab_manager =
      g_browser_process->GetTabManager();
  DCHECK(tab_manager);

  entry
      .SetHasBeforeUnloadHandler(
          web_contents->GetMainFrame()->GetSuddenTerminationDisablerState(
              blink::kBeforeUnloadHandler))
      .SetHasFormEntry(
          web_contents->GetPageImportanceSignals().had_form_interaction)
      // TODO(michaelpg): This dependency should be reversed during the
      // resource_coordinator refactor: crbug.com/775644.
      .SetIsExtensionProtected(!tab_manager->IsTabAutoDiscardable(web_contents))
      .SetIsPinned(tab_strip_model->IsTabPinned(index))
      .Record(ukm_recorder);
}
