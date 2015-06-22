// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_STUN_FIELD_TRIAL_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_STUN_FIELD_TRIAL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "third_party/webrtc/base/network.h"

namespace rtc {
class PacketSocketFactory;
class SocketAddress;
}  // namespace rtc

namespace stunprober {
class StunProber;
}  // namespace stunprober

namespace content {

// Wait for 30 seconds to avoid high CPU usage during browser start-up which
// might affect the accuracy of the trial. The trial wakes up the browser every
// 1 ms for no more than 3 seconds to see if time has passed for sending next
// stun probe.
static const int kExperimentStartDelayMs = 30000;

// This will use |factory| to create sockets, send stun binding requests with
// various intervals as determined by |params|, observed the success rate and
// latency of the stun responses and report through UMA.
scoped_ptr<stunprober::StunProber> StartStunProbeTrial(
    const rtc::NetworkManager::NetworkList& networks,
    const std::string& params,
    rtc::PacketSocketFactory* factory);

// Parsing function to decode the '/' separated parameter string |params|.
CONTENT_EXPORT bool ParseStunProbeParameters(
    const std::string& params,
    int* requests_per_ip,
    int* interval_ms,
    int* shared_socket_mode,
    std::vector<rtc::SocketAddress>* servers);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_STUN_FIELD_TRIAL_H_
