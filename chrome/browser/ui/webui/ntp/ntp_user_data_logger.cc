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

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(NTPUserDataLogger);

// Helper macro to log a load time to UMA. There's no good reason why we don't
// use one of the standard UMA_HISTORAM_*_TIMES macros, but all their ranges are
// different, and it's not worth changing all the existing histograms.
#define UMA_HISTOGRAM_LOAD_TIME(name, sample)                      \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample,                         \
                             base::TimeDelta::FromMilliseconds(1), \
                             base::TimeDelta::FromSeconds(60), 100)

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
  switch (event) {
    case NTP_ALL_TILES_RECEIVED:
      tiles_received_time_ = time;
      break;
    case NTP_ALL_TILES_LOADED:
      EmitNtpStatistics(time);
      break;
  }
}

void NTPUserDataLogger::LogMostVisitedImpression(
    int position,
    ntp_tiles::NTPTileSource tile_source) {
  if ((position >= kNumMostVisited) || impression_was_logged_[position]) {
    return;
  }
  impression_was_logged_[position] = true;
  impression_tile_source_[position] = tile_source;
}

void NTPUserDataLogger::LogMostVisitedNavigation(
    int position,
    ntp_tiles::NTPTileSource tile_source) {
  ntp_tiles::metrics::RecordTileClick(position, tile_source,
                                      ntp_tiles::metrics::THUMBNAIL);

  // Records the action. This will be available as a time-stamped stream
  // server-side and can be used to compute time-to-long-dwell.
  content::RecordAction(base::UserMetricsAction("MostVisited_Clicked"));
}

NTPUserDataLogger::NTPUserDataLogger(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      impression_tile_source_(kNumMostVisited),
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
    tiles_received_time_ = base::TimeDelta();
    has_emitted_ = false;
  }
}

void NTPUserDataLogger::EmitNtpStatistics(base::TimeDelta load_time) {
  // We only send statistics once per page.
  if (has_emitted_) {
    return;
  }

  DVLOG(1) << "Emitting NTP load time: " << load_time << ", "
           << "number of tiles: " << impression_was_logged_.count();

  std::vector<ntp_tiles::metrics::TileImpression> tiles;
  bool has_server_side_suggestions = false;
  for (int i = 0; i < kNumMostVisited; i++) {
    if (!impression_was_logged_[i]) {
      break;
    }
    if (impression_tile_source_[i] ==
        ntp_tiles::NTPTileSource::SUGGESTIONS_SERVICE) {
      has_server_side_suggestions = true;
    }
    // No URL passed since we're not interested in favicon-related Rappor
    // metrics.
    tiles.emplace_back(impression_tile_source_[i],
                       ntp_tiles::metrics::THUMBNAIL, GURL());
  }

  // Not interested in Rappor metrics.
  ntp_tiles::metrics::RecordPageImpression(tiles, /*rappor_service=*/nullptr);

  UMA_HISTOGRAM_LOAD_TIME("NewTabPage.TilesReceivedTime", tiles_received_time_);
  UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime", load_time);

  // Split between ML (aka SuggestionsService) and MV (aka TopSites).
  if (has_server_side_suggestions) {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.TilesReceivedTime.MostLikely",
                            tiles_received_time_);
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.MostLikely", load_time);
  } else {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.TilesReceivedTime.MostVisited",
                            tiles_received_time_);
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.MostVisited", load_time);
  }

  // Split between Web and Local.
  if (ntp_url_.SchemeIsHTTPOrHTTPS()) {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.TilesReceivedTime.Web",
                            tiles_received_time_);
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.Web", load_time);
  } else {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.TilesReceivedTime.LocalNTP",
                            tiles_received_time_);
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.LocalNTP", load_time);
  }

  // Split between Startup and non-startup.
  if (during_startup_) {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.TilesReceivedTime.Startup",
                            tiles_received_time_);
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.Startup", load_time);
  } else {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.TilesReceivedTime.NewTab",
                            tiles_received_time_);
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.NewTab", load_time);
  }

  has_emitted_ = true;
  during_startup_ = false;
}
