// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SEARCH_NTP_LOGGING_EVENTS_H_
#define CHROME_COMMON_SEARCH_NTP_LOGGING_EVENTS_H_

// The different types of events that are logged from the NTP. This enum is used
// to transfer information from the NTP javascript to the renderer and is not
// used as a UMA enum histogram's logged value.
// Note: Keep in sync with browser/resources/local_ntp/most_visited_util.js
// and browser/resources/local_ntp/most_visited_single.js
enum NTPLoggingEventType {
  // The suggestion is coming from the server.
  NTP_SERVER_SIDE_SUGGESTION = 0,

  // The suggestion is coming from the client.
  NTP_CLIENT_SIDE_SUGGESTION = 1,

  // Indicates a tile was rendered, no matter if it's a thumbnail, a gray tile
  // or an external tile.
  NTP_TILE = 2,

  // The tile uses a local thumbnail image.
  // Deleted: NTP_THUMBNAIL_TILE = 3,

  // Used when no thumbnail is specified and a gray tile with the domain is used
  // as the main tile.
  // Deleted: NTP_GRAY_TILE = 4,

  // The visuals of that tile are handled externally by the page itself.
  // Deleted: NTP_EXTERNAL_TILE = 5,

  // There was an error in loading both the thumbnail image and the fallback
  // (if it was provided), resulting in a grey tile.
  // Deleted: NTP_THUMBNAIL_ERROR = 6,

  // Used a gray tile with the domain as the fallback for a failed thumbnail.
  // Deleted: NTP_GRAY_TILE_FALLBACK = 7,

  // The visuals of that tile's fallback are handled externally.
  // Deleted: NTP_EXTERNAL_TILE_FALLBACK = 8,

  // Deleted: NTP_MOUSEOVER = 9

  // A NTP Tile has finished loading (successfully or failing).
  NTP_TILE_LOADED = 10,

  NTP_EVENT_TYPE_LAST = NTP_TILE_LOADED
};

// The source of an NTP tile.
// Note: Keep in sync with browser/resources/local_ntp/most_visited_util.js and
// browser/resources/local_ntp/most_visited_single.js.
// TODO(treib): Merge this into MostVisitedSource from components/ntp_tiles.
enum class NTPLoggingTileSource {
  CLIENT = 0,
  SERVER = 1,
  LAST = SERVER
};

#endif  // CHROME_COMMON_SEARCH_NTP_LOGGING_EVENTS_H_
