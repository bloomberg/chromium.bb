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
  // loaded (in particular the thumbnail images). Logged by the single-iframe
  // version of the NTP, and also by the multi-iframe version, but only as a
  // workaround for crbug.com/698675.
  NTP_ALL_TILES_RECEIVED = 12,

  // Activated by clicking on the fakebox icon. Logged by Voice Search.
  NTP_VOICE_ACTION_ACTIVATE_FAKEBOX = 13,
  // Activated by keyboard shortcut.
  NTP_VOICE_ACTION_ACTIVATE_KEYBOARD = 14,
  // Close the voice overlay by a user's explicit action.
  NTP_VOICE_ACTION_CLOSE_OVERLAY = 15,
  // Submitted voice query.
  NTP_VOICE_ACTION_QUERY_SUBMITTED = 16,
  // Clicked on support link in error message.
  NTP_VOICE_ACTION_SUPPORT_LINK_CLICKED = 17,
  // Retried by clicking Try Again link.
  NTP_VOICE_ACTION_TRY_AGAIN_LINK = 18,
  // Retried by clicking microphone button.
  NTP_VOICE_ACTION_TRY_AGAIN_MIC_BUTTON = 19,
  // Errors received from the Speech Recognition API.
  NTP_VOICE_ERROR_NO_SPEECH = 20,
  NTP_VOICE_ERROR_ABORTED = 21,
  NTP_VOICE_ERROR_AUDIO_CAPTURE = 22,
  NTP_VOICE_ERROR_NETWORK = 23,
  NTP_VOICE_ERROR_NOT_ALLOWED = 24,
  NTP_VOICE_ERROR_SERVICE_NOT_ALLOWED = 25,
  NTP_VOICE_ERROR_BAD_GRAMMAR = 26,
  NTP_VOICE_ERROR_LANGUAGE_NOT_SUPPORTED = 27,
  NTP_VOICE_ERROR_NO_MATCH = 28,
  NTP_VOICE_ERROR_OTHER = 29,

  NTP_EVENT_TYPE_LAST = NTP_VOICE_ERROR_OTHER
};

#endif  // CHROME_COMMON_SEARCH_NTP_LOGGING_EVENTS_H_
