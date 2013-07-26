// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_MOST_VISITED_TILES_EXPERIMENT_H_
#define CHROME_BROWSER_HISTORY_MOST_VISITED_TILES_EXPERIMENT_H_

#include "base/values.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/instant_types.h"

namespace history {

// This enum is also defined in histograms.xml. These values represent the
// types of actions carried out by the Most Visited Tile Placement experiment.
enum NtpTileExperimentActions {
  NTP_TILE_EXPERIMENT_ACTION_REMOVED_URL = 0,
  NTP_TILE_EXPERIMENT_ACTION_DID_NOT_REMOVE_URL = 1,
  NTP_TILE_EXPERIMENT_ACTION_TOO_FEW_URLS_TILES_1_8 = 2,
  NTP_TILE_EXPERIMENT_ACTION_TOO_FEW_URLS_TILES_1_4 = 3,
  // The number of Most Visited Tile Placement experiment actions logged.
  NUM_NTP_TILE_EXPERIMENT_ACTIONS
};

// Class for implementing the Most Visited Tile Placement experiment.
class MostVisitedTilesExperiment {
 public:
  // Helper method to shuffle MostVisited tiles for A/B testing purposes.
  static void MaybeShuffle(MostVisitedURLList* data);

  // Returns true if this user is part of the Most Visited Tile Placement
  // experiment group where URLs currently open in another browser tab are not
  // displayed on an NTP tile. Note: the experiment targets only the top-level
  // of sites i.e. if www.foo.com/bar is open in browser, and www.foo.com is a
  // recommended URL, www.foo.com will still appear on the next NTP open. The
  // experiment will not remove a URL if doing so would cause the number of Most
  // Visited recommendations to drop below eight.
  static bool IsDontShowOpenURLsEnabled();

  // Removes URLs already open in browser, for 1993 clients, if part of
  // experiment described for IsDontShowOpenURLsEnabled().
  static void RemoveItemsMatchingOpenTabs(
      const std::set<std::string>& open_urls,
      std::vector<InstantMostVisitedItem>* items);

  // Removes URLs already open in browser, for non-1993 clients, if part of
  // experiment described for IsDontShowOpenURLsEnabled().
  static void RemovePageValuesMatchingOpenTabs(
      const std::set<std::string>& open_urls,
      base::ListValue* pages_value);

 private:
  // Helper method to log the actions carried out by the Most Visited Tile
  // Placement experiment.
  static void LogInHistogram(NtpTileExperimentActions action);

  // Helper method to determine whether |url| is in |open_urls|.
  static bool ShouldRemoveURL(const std::set<std::string>& open_urls,
                       const std::string& url,
                       const size_t size);

  DISALLOW_IMPLICIT_CONSTRUCTORS(MostVisitedTilesExperiment);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_MOST_VISITED_TILES_EXPERIMENT_H_
