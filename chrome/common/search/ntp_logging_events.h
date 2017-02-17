// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SEARCH_NTP_LOGGING_EVENTS_H_
#define CHROME_COMMON_SEARCH_NTP_LOGGING_EVENTS_H_

// The different types of events that are logged from the NTP. This enum is used
// to transfer information from the NTP javascript to the renderer and is *not*
// used as a UMA enum histogram's logged value.
// Note: Keep in sync with browser/resources/local_ntp/most_visited_single.js
enum NTPLoggingEventType {
  // Deleted: NTP_SERVER_SIDE_SUGGESTION = 0,
  // Deleted: NTP_CLIENT_SIDE_SUGGESTION = 1,
  // Deleted: NTP_TILE = 2,
  // Deleted: NTP_THUMBNAIL_TILE = 3,
  // Deleted: NTP_GRAY_TILE = 4,
  // Deleted: NTP_EXTERNAL_TILE = 5,
  // Deleted: NTP_THUMBNAIL_ERROR = 6,
  // Deleted: NTP_GRAY_TILE_FALLBACK = 7,
  // Deleted: NTP_EXTERNAL_TILE_FALLBACK = 8,
  // Deleted: NTP_MOUSEOVER = 9
  // Deleted: NTP_TILE_LOADED = 10,

  // All NTP tiles have finished loading (successfully or failing). Logged only
  // by the single-iframe version of the NTP.
  NTP_ALL_TILES_LOADED = 11,

  // The data for all NTP tiles (title, URL, etc, but not the thumbnail image)
  // has been received by the most visited iframe. In contrast to
  // NTP_ALL_TILES_LOADED, this is recorded before the actual DOM elements have
  // loaded (in particular the thumbnail images). Logged only by the
  // single-iframe version of the NTP.
  NTP_ALL_TILES_RECEIVED = 12,

  NTP_EVENT_TYPE_LAST = NTP_ALL_TILES_RECEIVED
};

#endif  // CHROME_COMMON_SEARCH_NTP_LOGGING_EVENTS_H_
