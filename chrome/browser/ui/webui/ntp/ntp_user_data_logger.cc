// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"

#include <algorithm>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/search/search_urls.h"
#include "chrome/common/url_constants.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/ntp_tiles/metrics.h"
#include "components/sync_sessions/sessions_sync_manager.h"
#include "components/sync_sessions/sync_sessions_metrics.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"

namespace {

void RecordSyncSessionMetrics(content::WebContents* contents) {
  if (!contents)
    return;
  browser_sync::ProfileSyncService* sync =
      ProfileSyncServiceFactory::GetForProfile(
          Profile::FromBrowserContext(contents->GetBrowserContext()));
  if (!sync)
    return;
  sync_sessions::SessionsSyncManager* sessions =
      static_cast<sync_sessions::SessionsSyncManager*>(
          sync->GetSessionsSyncableService());
  sync_sessions::SyncSessionsMetrics::RecordYoungestForeignTabAgeOnNTP(
      sessions);
}

ntp_tiles::NTPTileSource ConvertTileSource(NTPLoggingTileSource tile_source) {
  switch (tile_source) {
    case NTPLoggingTileSource::CLIENT:
      return ntp_tiles::NTPTileSource::TOP_SITES;
    case NTPLoggingTileSource::SERVER:
      return ntp_tiles::NTPTileSource::SUGGESTIONS_SERVICE;
  }
  NOTREACHED();
  return ntp_tiles::NTPTileSource::TOP_SITES;
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(NTPUserDataLogger);


// Log a time event for a given |histogram| at a given |value|. This
// routine exists because regular histogram macros are cached thus can't be used
// if the name of the histogram will change at a given call site.
void LogLoadTimeHistogram(const std::string& histogram, base::TimeDelta value) {
  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      histogram,
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(60), 100,
      base::Histogram::kUmaTargetedHistogramFlag);
  if (counter)
    counter->AddTime(value);
}


NTPUserDataLogger::~NTPUserDataLogger() {}

// static
NTPUserDataLogger* NTPUserDataLogger::GetOrCreateFromWebContents(
      content::WebContents* content) {
  DCHECK(search::IsInstantNTP(content));

  // Calling CreateForWebContents when an instance is already attached has no
  // effect, so we can do this.
  NTPUserDataLogger::CreateForWebContents(content);
  NTPUserDataLogger* logger = NTPUserDataLogger::FromWebContents(content);

  // We record the URL of this NTP in order to identify navigations that
  // originate from it. We use the NavigationController's URL since it might
  // differ from the WebContents URL which is usually chrome://newtab/.
  //
  // We update the NTP URL every time this function is called, because the NTP
  // URL sometimes changes while it is open, and we care about the final one for
  // detecting when the user leaves or returns to the NTP. In particular, if the
  // Google URL changes (e.g. google.com -> google.de), then we fall back to the
  // local NTP.
  const content::NavigationEntry* entry =
      content->GetController().GetVisibleEntry();
  if (entry && (logger->ntp_url_ != entry->GetURL())) {
    DVLOG(1) << "NTP URL changed from \"" << logger->ntp_url_ << "\" to \""
             << entry->GetURL() << "\"";
    logger->ntp_url_ = entry->GetURL();
  }

  return logger;
}

void NTPUserDataLogger::LogEvent(NTPLoggingEventType event,
                                 base::TimeDelta time) {
  DCHECK_EQ(NTP_ALL_TILES_LOADED, event);
  EmitNtpStatistics(time);
}

void NTPUserDataLogger::LogMostVisitedImpression(
    int position, NTPLoggingTileSource tile_source) {
  if ((position >= kNumMostVisited) || impression_was_logged_[position]) {
    return;
  }
  impression_was_logged_[position] = true;

  switch (tile_source) {
    case NTPLoggingTileSource::CLIENT:
      has_client_side_suggestions_ = true;
      break;
    case NTPLoggingTileSource::SERVER:
      has_server_side_suggestions_ = true;
      break;
  }

  ntp_tiles::metrics::RecordTileImpression(position,
                                           ConvertTileSource(tile_source));
}

void NTPUserDataLogger::LogMostVisitedNavigation(
    int position, NTPLoggingTileSource tile_source) {
  ntp_tiles::metrics::RecordTileClick(position, ConvertTileSource(tile_source),
                                      ntp_tiles::metrics::THUMBNAIL);

  // Records the action. This will be available as a time-stamped stream
  // server-side and can be used to compute time-to-long-dwell.
  content::RecordAction(base::UserMetricsAction("MostVisited_Clicked"));
}

NTPUserDataLogger::NTPUserDataLogger(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      has_server_side_suggestions_(false),
      has_client_side_suggestions_(false),
      has_emitted_(false),
      during_startup_(!AfterStartupTaskUtils::IsBrowserStartupComplete()) {
  // We record metrics about session data here because when this class typically
  // emits metrics it is too late. This session data would theoretically have
  // been used to populate the page, and we want to learn about its state when
  // the NTP is being generated.
  RecordSyncSessionMetrics(contents);
}

// content::WebContentsObserver override
void NTPUserDataLogger::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  NavigatedFromURLToURL(load_details.previous_url,
                        load_details.entry->GetURL());
}

void NTPUserDataLogger::NavigatedFromURLToURL(const GURL& from,
                                              const GURL& to) {
  // User is returning to NTP, probably via the back button; reset stats.
  if (from.is_valid() && to.is_valid() && (to == ntp_url_)) {
    DVLOG(1) << "Returning to New Tab Page";
    impression_was_logged_.reset();
    has_emitted_ = false;
    has_server_side_suggestions_ = false;
    has_client_side_suggestions_ = false;
  }
}

void NTPUserDataLogger::EmitNtpStatistics(base::TimeDelta load_time) {
  // We only send statistics once per page.
  if (has_emitted_)
    return;

  size_t number_of_tiles = impression_was_logged_.count();
  DVLOG(1) << "Emitting NTP load time: " << load_time << ", "
           << "number of tiles: " << number_of_tiles;

  ntp_tiles::NTPTileSource source =
      has_server_side_suggestions_
          ? ntp_tiles::NTPTileSource::SUGGESTIONS_SERVICE
          : ntp_tiles::NTPTileSource::TOP_SITES;
  std::vector<std::pair<ntp_tiles::NTPTileSource,
                        ntp_tiles::metrics::MostVisitedTileType>>
      tiles(number_of_tiles,
            std::make_pair(source, ntp_tiles::metrics::THUMBNAIL));
  ntp_tiles::metrics::RecordPageImpression(tiles);

  LogLoadTimeHistogram("NewTabPage.LoadTime", load_time);

  // Split between ML and MV.
  std::string type = has_server_side_suggestions_ ?
      "MostLikely" : "MostVisited";
  LogLoadTimeHistogram("NewTabPage.LoadTime." + type, load_time);

  // Split between Web and Local.
  std::string variant = ntp_url_.SchemeIsHTTPOrHTTPS() ? "Web" : "LocalNTP";
  LogLoadTimeHistogram("NewTabPage.LoadTime." + variant, load_time);

  // Split between Startup and non-startup.
  std::string status = during_startup_ ? "Startup" : "NewTab";
  LogLoadTimeHistogram("NewTabPage.LoadTime." + status, load_time);

  has_emitted_ = true;
  during_startup_ = false;
}
