// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_UMA_HISTOGRAMS_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_UMA_HISTOGRAMS_H_

#include "base/metrics/histogram.h"
#include "base/strings/string16.h"

namespace content {

// Helper enum used for histogramming calls to WebRTC APIs from JavaScript.
enum JavaScriptAPIName {
  WEBKIT_GET_USER_MEDIA,
  WEBKIT_PEER_CONNECTION,
  WEBKIT_DEPRECATED_PEER_CONNECTION,
  WEBKIT_RTC_PEER_CONNECTION,
  INVALID_NAME
};

// Helper method used to collect information about the raw count of
// the number of times different WebRTC APIs are called from
// JavaScript.
//
// The histogram can be viewed at
// chrome://histograms/WebRTC.webkitApiCount.
void UpdateWebRTCMethodCount(JavaScriptAPIName api_name);

// Helper method used to collect information about the number of
// unique security origins that call different WebRTC APIs within the
// current browser session. For example, if abc.com calls getUserMedia
// 100 times and RTCPeerConnection 10 times, and xyz.com calls
// getUserMedia 50 times, RTCPeerConnection will have a count of 1 and
// getUserMedia will have a count of 2.
//
// The histogram can be viewed at
// chrome://histograms/WebRTC.webkitApiCountUniqueByOrigin.
void UpdateWebRTCUniqueOriginMethodCount(JavaScriptAPIName api_name,
                                         const base::string16& security_origin);

} //  namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_UMA_HISTOGRAMS_H_
