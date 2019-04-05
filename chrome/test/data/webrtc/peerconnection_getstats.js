/**
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Maps "RTCStats.type" values to descriptions of whitelisted (allowed to be
 * exposed to the web) RTCStats-derived dictionaries described below.
 * @private
 */
let gStatsWhitelist = new Map();

/**
 * RTCRtpStreamStats
 * https://w3c.github.io/webrtc-stats/#streamstats-dict*
 * @private
 */
let kRTCRtpStreamStats = new RTCStats(null, {
  ssrc: 'number',
  isRemote: 'boolean',  // Obsolete, type reveals if "remote-" or not.
  kind: 'string',
  mediaType: 'string',  // Obsolete, replaced by |kind|.
  transportId: 'string',
  codecId: 'string',
});

/**
 * RTCReceivedRtpStreamStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcreceivedrtpstreamstats
 * @private
 */
let kRTCReceivedRtpStreamStats = new RTCStats(kRTCRtpStreamStats, {
  packetsReceived: 'number',
  packetsLost: 'number',
  jitter: 'number',
  packetsDiscarded: 'number',
  packetsRepaired: 'number',
  burstPacketsLost: 'number',
  burstPacketsDiscarded: 'number',
  burstLossCount: 'number',
  burstDiscardCount: 'number',
  burstLossRate: 'number',
  burstDiscardRate: 'number',
  gapLossRate: 'number',
  gapDiscardRate: 'number',
});

/*
 * RTCInboundRTPStreamStats
 * https://w3c.github.io/webrtc-stats/#inboundrtpstats-dict*
 * @private
 */
let kRTCInboundRtpStreamStats = new RTCStats(kRTCReceivedRtpStreamStats, {
  trackId: 'string',
  receiverId: 'string',
  remoteId: 'string',
  framesDecoded: 'number',
  qpSum: 'number',
  lastPacketReceivedTimestamp: 'number',
  averageRtcpInterval: 'number',
  fecPacketsReceived: 'number',
  fecPacketsDiscarded: 'number',
  bytesReceived: 'number',
  packetsFailedDecryption: 'number',
  packetsDuplicated: 'number',
  perDscpPacketsReceived: 'object',
  nackCount: 'number',
  firCount: 'number',
  pliCount: 'number',
  sliCount: 'number',
  fractionLost: 'number',  // Obsolete, moved to RTCRemoteInboundRtpStreamStats.
});
gStatsWhitelist.set('inbound-rtp', kRTCInboundRtpStreamStats);

/*
 * RTCRemoteInboundRtpStreamStats
 * https://w3c.github.io/webrtc-stats/#remoteinboundrtpstats-dict*
 * @private
 */
let kRTCRemoteInboundRtpStreamStats =
    new RTCStats(kRTCReceivedRtpStreamStats, {
  localId: 'string',
  roundTripTime: 'number',
  fractionLost: 'number',
});
// TODO(hbos): Add this to the whitelist when it has been implemented. Adding it
// before then will tag it as missing.
// gStatsWhitelist.set('remote-inbound-rtp', kRTCRemoteInboundRtpStreamStats);

/**
 * RTCSentRtpStreamStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcsentrtpstreamstats
 * @private
 */
let kRTCSentRtpStreamStats = new RTCStats(kRTCRtpStreamStats, {
  packetsSent: 'number',
  packetsDiscardedOnSend: 'number',
  fecPacketsSent: 'number',
  bytesSent: 'number',
  bytesDiscardedOnSend: 'number',
});

/*
 * RTCOutboundRtpStreamStats
 * https://w3c.github.io/webrtc-stats/#outboundrtpstats-dict*
 * @private
 */
let kRTCOutboundRtpStreamStats = new RTCStats(kRTCSentRtpStreamStats, {
  trackId: 'string',
  senderId: 'string',
  remoteId: 'string',
  lastPacketSentTimestamp: 'number',
  retransmittedPacketsSent: 'number',
  retransmittedBytesSent: 'number',
  targetBitrate: 'number',
  totalEncodedBytesTarget: 'number',
  framesEncoded: 'number',
  qpSum: 'number',
  totalEncodeTime: 'number',
  averageRtcpInterval: 'number',
  qualityLimitationReason: 'string',
  qualityLimitationDurations: 'object',
  perDscpPacketsSent: 'object',
  nackCount: 'number',
  firCount: 'number',
  pliCount: 'number',
  sliCount: 'number',
});
gStatsWhitelist.set('outbound-rtp', kRTCOutboundRtpStreamStats);

/*
 * RTCRemoteOutboundRtpStreamStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcremoteoutboundrtpstreamstats
 * @private
 */
let kRTCRemoteOutboundRtpStreamStats = new RTCStats(kRTCSentRtpStreamStats, {
  localId: 'string',
  remoteTimestamp: 'number',
});
// TODO(hbos): Add this to the whitelist when it has been implemented. Adding it
// before then will tag it as missing.
// gStatsWhitelist.set('remote-outbound-rtp', kRTCRemoteOutboundRtpStreamStats);

/*
 * RTCRtpContributingSourceStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcrtpcontributingsourcestats
 * @private
 */
let kRTCRtpContributingSourceStats = new RTCStats(null, {
  contributorSsrc: 'number',
  inboundRtpStreamId: 'string',
  packetsContributedTo: 'number',
  audioLevel: 'number',
});
// TODO(hbos): Add this to the whitelist when it has been implemented. Adding it
// before then will tag it as missing.
// gStatsWhitelist.set('csrc', kRTCRtpContributingSourceStats);

/*
 * RTCCodecStats
 * https://w3c.github.io/webrtc-stats/#codec-dict*
 * @private
 */
let kRTCCodecStats = new RTCStats(null, {
  payloadType: 'number',
  mimeType: 'string',
  // TODO(hbos): As soon as |codec| has been renamed |mimeType| in the webrtc
  // repo, remove this line. https://bugs.webrtc.org/7061
  codec: 'string',
  clockRate: 'number',
  channels: 'number',
  sdpFmtpLine: 'string',
  implementation: 'string',
});
gStatsWhitelist.set('codec', kRTCCodecStats);

/*
 * RTCPeerConnectionStats
 * https://w3c.github.io/webrtc-stats/#pcstats-dict*
 * @private
 */
let kRTCPeerConnectionStats = new RTCStats(null, {
  dataChannelsOpened: 'number',
  dataChannelsClosed: 'number',
  dataChannelsRequested: 'number',
  dataChannelsAccepted: 'number',
});
gStatsWhitelist.set('peer-connection', kRTCPeerConnectionStats);

/*
 * RTCMediaStreamStats
 * https://w3c.github.io/webrtc-stats/#msstats-dict*
 * @private
 */
let kRTCMediaStreamStats = new RTCStats(null, {
  streamIdentifier: 'string',
  trackIds: 'sequence_string',
});
gStatsWhitelist.set('stream', kRTCMediaStreamStats);

/*
 * RTCMediaStreamTrackStats
 * https://w3c.github.io/webrtc-stats/#mststats-dict*
 * @private
 */
let kRTCMediaStreamTrackStats = new RTCStats(null, {
  trackIdentifier: 'string',
  remoteSource: 'boolean',
  ended: 'boolean',
  detached: 'boolean',
  kind: 'string',
  estimatedPlayoutTimestamp: 'number',
  frameWidth: 'number',
  frameHeight: 'number',
  framesPerSecond: 'number',
  framesCaptured: 'number',
  framesSent: 'number',
  hugeFramesSent: 'number',
  framesReceived: 'number',
  framesDecoded: 'number',
  framesDropped: 'number',
  framesCorrupted: 'number',
  partialFramesLost: 'number',
  fullFramesLost: 'number',
  audioLevel: 'number',
  totalAudioEnergy: 'number',
  voiceActivityFlag: 'boolean',
  echoReturnLoss: 'number',
  echoReturnLossEnhancement: 'number',
  totalSamplesSent: 'number',
  totalSamplesReceived: 'number',
  totalSamplesDuration: 'number',
  concealedSamples: 'number',
  concealmentEvents: 'number',
  jitterBufferDelay: 'number',
  jitterBufferEmittedCount: 'number',
  priority: 'string'
});
gStatsWhitelist.set('track', kRTCMediaStreamTrackStats);

/*
 * RTCDataChannelStats
 * https://w3c.github.io/webrtc-stats/#dcstats-dict*
 * @private
 */
let kRTCDataChannelStats = new RTCStats(null, {
  label: 'string',
  protocol: 'string',
  datachannelid: 'number',
  state: 'string',
  messagesSent: 'number',
  bytesSent: 'number',
  messagesReceived: 'number',
  bytesReceived: 'number',
});
gStatsWhitelist.set('data-channel', kRTCDataChannelStats);

/*
 * RTCTransportStats
 * https://w3c.github.io/webrtc-stats/#transportstats-dict*
 * @private
 */
let kRTCTransportStats = new RTCStats(null, {
  bytesSent: 'number',
  bytesReceived: 'number',
  rtcpTransportStatsId: 'string',
  dtlsState: 'string',
  selectedCandidatePairId: 'string',
  localCertificateId: 'string',
  remoteCertificateId: 'string',
});
gStatsWhitelist.set('transport', kRTCTransportStats);

/*
 * RTCIceCandidateStats
 * https://w3c.github.io/webrtc-stats/#icecandidate-dict*
 * @private
 */
let kRTCIceCandidateStats = new RTCStats(null, {
  transportId: 'string',
  isRemote: 'boolean',
  networkType: 'string',
  ip: 'string',
  port: 'number',
  protocol: 'string',
  relayProtocol: 'string',
  candidateType: 'string',
  priority: 'number',
  url: 'string',
  deleted: 'boolean',
});
gStatsWhitelist.set('local-candidate', kRTCIceCandidateStats);
gStatsWhitelist.set('remote-candidate', kRTCIceCandidateStats);

/*
 * RTCIceCandidatePairStats
 * https://w3c.github.io/webrtc-stats/#candidatepair-dict*
 * @private
 */
let kRTCIceCandidatePairStats = new RTCStats(null, {
  transportId: 'string',
  localCandidateId: 'string',
  remoteCandidateId: 'string',
  state: 'string',
  priority: 'number',
  nominated: 'boolean',
  writable: 'boolean',
  readable: 'boolean',
  bytesSent: 'number',
  bytesReceived: 'number',
  totalRoundTripTime: 'number',
  currentRoundTripTime: 'number',
  availableOutgoingBitrate: 'number',
  availableIncomingBitrate: 'number',
  requestsReceived: 'number',
  requestsSent: 'number',
  responsesReceived: 'number',
  responsesSent: 'number',
  retransmissionsReceived: 'number',
  retransmissionsSent: 'number',
  consentRequestsReceived: 'number',
  consentRequestsSent: 'number',
  consentResponsesReceived: 'number',
  consentResponsesSent: 'number',
});
gStatsWhitelist.set('candidate-pair', kRTCIceCandidatePairStats);

/*
 * RTCCertificateStats
 * https://w3c.github.io/webrtc-stats/#certificatestats-dict*
 * @private
 */
let kRTCCertificateStats = new RTCStats(null, {
  fingerprint: 'string',
  fingerprintAlgorithm: 'string',
  base64Certificate: 'string',
  issuerCertificateId: 'string',
});
gStatsWhitelist.set('certificate', kRTCCertificateStats);

// Public interface to tests. These are expected to be called with
// ExecuteJavascript invocations from the browser tests and will return answers
// through the DOM automation controller.

/**
 * Verifies that the promise-based |RTCPeerConnection.getStats| returns stats,
 * makes sure that all returned stats have the base RTCStats-members and that
 * all stats are allowed by the whitelist.
 *
 * Returns "ok-" followed by JSON-stringified array of "RTCStats.type" values
 * to the test, these being the different types of stats that was returned by
 * this call to getStats.
 */
function verifyStatsGeneratedPromise() {
  peerConnection_().getStats()
    .then(function(report) {
      if (report == null || report.size == 0)
        throw new failTest('report is null or empty.');
      let statsTypes = new Set();
      let ids = new Set();
      for (let stats of report.values()) {
        verifyStatsIsWhitelisted_(stats);
        statsTypes.add(stats.type);
        if (ids.has(stats.id))
          throw failTest('stats.id is not a unique identifier.');
        ids.add(stats.id);
      }
      returnToTest('ok-' + JSON.stringify(Array.from(statsTypes.values())));
    },
    function(e) {
      throw failTest('Promise was rejected: ' + e);
    });
}

/**
 * Gets the result of the promise-based |RTCPeerConnection.getStats| as a
 * dictionary of RTCStats-dictionaries.
 *
 * Returns "ok-" followed by a JSON-stringified dictionary of dictionaries to
 * the test.
 */
function getStatsReportDictionary() {
  peerConnection_().getStats()
    .then(function(report) {
      if (report == null || report.size == 0)
        throw new failTest('report is null or empty.');
      let reportDictionary = {};
      for (let stats of report.values()) {
        reportDictionary[stats.id] = stats;
      }
      returnToTest('ok-' + JSON.stringify(reportDictionary));
    },
    function(e) {
      throw failTest('Promise was rejected: ' + e);
    });
}

/**
 * Measures the performance of the promise-based |RTCPeerConnection.getStats|
 * and returns the time it took in milliseconds as a double
 * (DOMHighResTimeStamp, accurate to one thousandth of a millisecond).
 * Verifies that all stats types of the whitelist were contained in the result.
 *
 * Returns "ok-" followed by a double on success.
 */
function measureGetStatsPerformance() {
  let t0 = performance.now();
  peerConnection_().getStats()
    .then(function(report) {
      let t1 = performance.now();
      let statsTypes = new Set();
      for (let stats of report.values()) {
        verifyStatsIsWhitelisted_(stats);
        statsTypes.add(stats.type);
      }
      if (statsTypes.size != gStatsWhitelist.size) {
        returnToTest('The returned report contains a different number (' +
            statsTypes.size + ') of stats types than the whitelist. ' +
            JSON.stringify(Array.from(statsTypes)));
      }
      returnToTest('ok-' + (t1 - t0));
    },
    function(e) {
      throw failTest('Promise was rejected: ' + e);
    });
}

/**
 * Returns a complete list of whitelisted "RTCStats.type" values as a
 * JSON-stringified array of strings to the test.
 */
function getWhitelistedStatsTypes() {
  returnToTest(JSON.stringify(Array.from(gStatsWhitelist.keys())));
}

// Internals.

/** @private */
function RTCStats(parent, membersObject) {
  if (parent != null)
    Object.assign(this, parent);
  Object.assign(this, membersObject);
}

/**
 * Checks if |stats| correctly maps to a a whitelisted RTCStats-derived
 * dictionary, throwing |failTest| if it doesn't. See |gStatsWhitelist|.
 *
 * The "RTCStats.type" must map to a known dictionary description. Every member
 * is optional, but if present it must be present in the whitelisted dictionary
 * description and its type must match.
 * @private
 */
function verifyStatsIsWhitelisted_(stats) {
  if (stats == null)
    throw failTest('stats is null or undefined: ' + stats);
  if (typeof(stats.id) !== 'string')
    throw failTest('stats.id is not a string:' + stats.id);
  if (typeof(stats.timestamp) !== 'number' || !isFinite(stats.timestamp) ||
      stats.timestamp <= 0) {
    throw failTest('stats.timestamp is not a positive finite number: ' +
        stats.timestamp);
  }
  if (typeof(stats.type) !== 'string')
    throw failTest('stats.type is not a string: ' + stats.type);
  let whitelistedStats = gStatsWhitelist.get(stats.type);
  if (whitelistedStats == null)
    throw failTest('stats.type is not a whitelisted type: ' + stats.type);
  for (let propertyName in stats) {
    if (propertyName === 'id' || propertyName === 'timestamp' ||
        propertyName === 'type') {
      continue;
    }
    if (!whitelistedStats.hasOwnProperty(propertyName)) {
      throw failTest(
          stats.type + '.' + propertyName + ' is not a whitelisted ' +
          'member: ' + stats[propertyName]);
    }
    if (whitelistedStats[propertyName] === 'any')
      continue;
    if (!whitelistedStats[propertyName].startsWith('sequence_')) {
      if (typeof(stats[propertyName]) !== whitelistedStats[propertyName]) {
        throw failTest('stats.' + propertyName + ' should have a different ' +
            'type according to the whitelist: ' + stats[propertyName] + ' vs ' +
            whitelistedStats[propertyName]);
      }
    } else {
      if (!Array.isArray(stats[propertyName])) {
        throw failTest('stats.' + propertyName + ' should have a different ' +
            'type according to the whitelist (should be an array): ' +
            JSON.stringify(stats[propertyName]) + ' vs ' +
            whitelistedStats[propertyName]);
      }
      let elementType = whitelistedStats[propertyName].substring(9);
      for (let element in stats[propertyName]) {
        if (typeof(element) !== elementType) {
          throw failTest('stats.' + propertyName + ' should have a different ' +
              'type according to the whitelist (an element of the array has ' +
              'the incorrect type): ' + JSON.stringify(stats[propertyName]) +
              ' vs ' + whitelistedStats[propertyName]);
        }
      }
    }
  }
}
