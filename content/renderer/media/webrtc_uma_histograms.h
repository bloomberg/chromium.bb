// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_UMA_HISTOGRAMS_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_UMA_HISTOGRAMS_H_

#include "base/metrics/histogram.h"

namespace {

// Helper enum used for histogramming calls to WebRTC APIs from JavaScript.
enum JavaScriptAPIName {
  WEBKIT_GET_USER_MEDIA,
  WEBKIT_PEER_CONNECTION,
  WEBKIT_DEPRECATED_PEER_CONNECTION,
  WEBKIT_RTC_PEER_CONNECTION,
  INVALID_NAME
};

// Helper method used to collect information about the number of times
// different WebRTC API:s are called from JavaScript.
// The histogram can be viewed at chrome://histograms/WebRTC.webkitApiCount.
void UpdateWebRTCMethodCount(JavaScriptAPIName api_name) {
  UMA_HISTOGRAM_ENUMERATION("WebRTC.webkitApiCount", api_name, INVALID_NAME);
}

} //  namespace

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_UMA_HISTOGRAMS_H_
