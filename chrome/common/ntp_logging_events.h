// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NTP_LOGGING_EVENTS_H_
#define CHROME_COMMON_NTP_LOGGING_EVENTS_H_

// The different types of events that are logged from the NTP.
enum NTPLoggingEventType {
  // The user moused over an NTP tile or title.
  NTP_MOUSEOVER = 0,

  // The page attempted to load a thumbnail image.
  NTP_THUMBNAIL_ATTEMPT = 1,

  // There was an error in loading both the thumbnail image and the fallback
  // (if it was provided), resulting in a grey tile.
  NTP_THUMBNAIL_ERROR = 2,

  // The page attempted to load a thumbnail URL while a fallback thumbnail was
  // provided.
  NTP_FALLBACK_THUMBNAIL_REQUESTED = 3,

  // The primary thumbnail image failed to load and caused us to use the
  // secondary thumbnail as a fallback.
  NTP_FALLBACK_THUMBNAIL_USED = 4,

  // The suggestion is coming from the server.
  NTP_SERVER_SIDE_SUGGESTION = 5,

  // The suggestion is coming from the client.
  NTP_CLIENT_SIDE_SUGGESTION = 6,

  // The visuals of that tile are handled externally by the page itself.
  NTP_EXTERNAL_TILE = 7,

  NTP_NUM_EVENT_TYPES
};

#endif  // CHROME_COMMON_NTP_LOGGING_EVENTS_H_
