// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_metrics_logger.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_importance_signals.h"
#include "net/base/mime_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/WebKit/public/platform/WebSuddenTerminationDisablerType.h"
#include "url/gurl.h"

using metrics::TabMetricsEvent;

namespace {

// Order must match the metrics.TabMetricsEvent.Scheme enum.
const char* kWhitelistedSchemes[] = {
    "",  // Placeholder for PROTOCOL_HANDLER_SCHEME_OTHER.
    "bitcoin", "geo",  "im",   "irc",         "ircs", "magnet", "mailto",
    "mms",     "news", "nntp", "openpgp4fpr", "sip",  "sms",    "smsto",
    "ssh",     "tel",  "urn",  "webcal",      "wtai", "xmpp",
};

// Adds a DefaultProtocolHandler metric with the handler's scheme to |entry|
// if the protocol handler is a default protocol handler.
void PopulateSchemeForHandler(ProtocolHandlerRegistry* registry,
                              const ProtocolHandler& handler,
                              ukm::builders::TabManager_TabMetrics* entry) {
  if (registry->IsDefault(handler)) {
    // Append a DefaultProtocolHandler metric whose value is the scheme.
    // Note that multiple DefaultProtocolHandler metrics may be added, one for
    // each scheme the entry's origin handles by default.
    entry->SetDefaultProtocolHandler(
        TabMetricsLogger::GetSchemeValueFromString(handler.protocol()));
  }
}

// Populates the protocol handler fields based on the WebContents' origin.
// We match the origin instead of the full page URL because:
// - a handler relevant for one page is probably relevant for the whole site
// - a handler maps to a template string, so matching on a full URL is hard
// - even if this page was opened from a protocol handler, a redirect may have
//   changed the URL anyway.
void PopulateProtocolHandlers(content::WebContents* web_contents,
                              ukm::builders::TabManager_TabMetrics* entry) {
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  // May be null in tests.
  if (!registry)
    return;

  const GURL origin = web_contents->GetLastCommittedURL().GetOrigin();
  if (origin.is_empty())
    return;

  // Fetch all schemes that have been registered (accepted or denied).
  std::vector<std::string> registered_schemes;
  registry->GetRegisteredProtocols(&registered_schemes);

  // Protocol handlers are stored by scheme, not URL. For each scheme, find the
  // URLs of the handlers registered for it.
  for (const std::string& scheme : registered_schemes) {
    for (const ProtocolHandler& handler : registry->GetHandlersFor(scheme)) {
      if (handler.url().GetOrigin() == origin)
        PopulateSchemeForHandler(registry, handler, entry);
    }
  }
}

// Populates navigation-related metrics.
void PopulatePageTransitionMetrics(ukm::builders::TabManager_TabMetrics* entry,
                                   ui::PageTransition page_transition) {
  // We only report the following core types.
  // Note: Redirects unrelated to clicking a link still get the "link" type.
  if (ui::PageTransitionCoreTypeIs(page_transition, ui::PAGE_TRANSITION_LINK) ||
      ui::PageTransitionCoreTypeIs(page_transition,
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
      ui::PageTransitionCoreTypeIs(page_transition,
                                   ui::PAGE_TRANSITION_FORM_SUBMIT) ||
      ui::PageTransitionCoreTypeIs(page_transition,
                                   ui::PAGE_TRANSITION_RELOAD)) {
    entry->SetPageTransitionCoreType(
        ui::PageTransitionStripQualifier(page_transition));
  }

  entry->SetPageTransitionFromAddressBar(
      (page_transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) != 0);
  entry->SetPageTransitionIsRedirect(
      ui::PageTransitionIsRedirect(page_transition));
}

// Logs the TabManager.Background.ForegroundedOrClosed event.
void LogBackgroundTabForegroundedOrClosed(
    ukm::SourceId ukm_source_id,
    base::TimeDelta inactive_duration,
    const TabMetricsLogger::MRUMetrics& mru_metrics,
    bool is_foregrounded) {
  if (!ukm_source_id)
    return;

  ukm::builders::TabManager_Background_ForegroundedOrClosed(ukm_source_id)
      .SetIsForegrounded(is_foregrounded)
      .SetMRUIndex(mru_metrics.index)
      .SetTimeFromBackgrounded(inactive_duration.InMilliseconds())
      .SetTotalTabCount(mru_metrics.total)
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace

TabMetricsLogger::TabMetricsLogger() = default;
TabMetricsLogger::~TabMetricsLogger() = default;

// static
TabMetricsEvent::ContentType TabMetricsLogger::GetContentTypeFromMimeType(
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

// static
TabMetricsEvent::ProtocolHandlerScheme
TabMetricsLogger::GetSchemeValueFromString(const std::string& scheme) {
  const char* const* const scheme_ptr = std::find(
      std::begin(kWhitelistedSchemes), std::end(kWhitelistedSchemes), scheme);
  if (scheme_ptr == std::end(kWhitelistedSchemes))
    return TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER;

  size_t index = scheme_ptr - std::begin(kWhitelistedSchemes);
  return static_cast<TabMetricsEvent::ProtocolHandlerScheme>(index);
}

// static
int TabMetricsLogger::GetSiteEngagementScore(
    const content::WebContents* web_contents) {
  if (!SiteEngagementService::IsEnabled())
    return -1;

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

void TabMetricsLogger::LogBackgroundTab(ukm::SourceId ukm_source_id,
                                        const TabMetrics& tab_metrics) {
  if (!ukm_source_id)
    return;

  content::WebContents* web_contents = tab_metrics.web_contents;

  // UKM recording is disabled in OTR.
  if (web_contents->GetBrowserContext()->IsOffTheRecord())
    return;

  // Verify that the browser is not closing.
  const Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser ||
      base::ContainsKey(
          BrowserList::GetInstance()->currently_closing_browsers(), browser)) {
    return;
  }

  const TabStripModel* tab_strip_model = browser->tab_strip_model();
  if (tab_strip_model->closing_all())
    return;

  int index = tab_strip_model->GetIndexOfWebContents(web_contents);
  DCHECK_NE(index, TabStripModel::kNoTab);

  ukm::builders::TabManager_TabMetrics entry(ukm_source_id);

  // The browser window logs its own usage UKMs with its session ID.
  entry.SetWindowId(browser->session_id().id());

  entry.SetKeyEventCount(tab_metrics.page_metrics.key_event_count)
      .SetMouseEventCount(tab_metrics.page_metrics.mouse_event_count)
      .SetTouchEventCount(tab_metrics.page_metrics.touch_event_count);

  PopulateProtocolHandlers(web_contents, &entry);

  const int engagement_score = GetSiteEngagementScore(web_contents);
  if (engagement_score >= 0) {
    entry.SetSiteEngagementScore(engagement_score);
  }

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

  PopulatePageTransitionMetrics(&entry, tab_metrics.page_transition);
  entry
      .SetHasBeforeUnloadHandler(
          web_contents->GetMainFrame()->GetSuddenTerminationDisablerState(
              blink::kBeforeUnloadHandler))
      .SetHasFormEntry(
          web_contents->GetPageImportanceSignals().had_form_interaction)
      // TODO(michaelpg): This dependency should be reversed during the
      // resource_coordinator refactor: https://crbug.com/775644.
      .SetIsExtensionProtected(!tab_manager->IsTabAutoDiscardable(web_contents))
      .SetIsPinned(tab_strip_model->IsTabPinned(index))
      .SetNavigationEntryCount(web_contents->GetController().GetEntryCount())
      .SetSequenceId(++sequence_id_)
      .Record(ukm::UkmRecorder::Get());
}

void TabMetricsLogger::LogBackgroundTabShown(ukm::SourceId ukm_source_id,
                                             base::TimeDelta inactive_duration,
                                             const MRUMetrics& mru_metrics) {
  LogBackgroundTabForegroundedOrClosed(ukm_source_id, inactive_duration,
                                       mru_metrics, true /* is_shown */);
}

void TabMetricsLogger::LogBackgroundTabClosed(ukm::SourceId ukm_source_id,
                                              base::TimeDelta inactive_duration,
                                              const MRUMetrics& mru_metrics) {
  LogBackgroundTabForegroundedOrClosed(ukm_source_id, inactive_duration,
                                       mru_metrics, false /* is_shown */);
}

void TabMetricsLogger::LogTabLifetime(ukm::SourceId ukm_source_id,
                                      base::TimeDelta time_since_navigation) {
  if (!ukm_source_id)
    return;
  ukm::builders::TabManager_TabLifetime(ukm_source_id)
      .SetTimeSinceNavigation(time_since_navigation.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}
