/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// TODO(phoglund): merge into message_handling.js.

/** @private */
var gHasSeenCryptoInSdp = 'no-crypto-seen';

// Public interface towards the other javascript files, such as
// message_handling.js. The contract for these functions is described in
// message_handling.js.

function receiveOffer(peerConnection, offer, constraints, callback) {
  var sessionDescription = new RTCSessionDescription(offer);
  peerConnection.setRemoteDescription(
      sessionDescription,
      function() { success_('setRemoteDescription'); },
      function() { failure_('setRemoteDescription'); });

  peerConnection.createAnswer(
      function(answer) {
        success_('createAnswer');
        setLocalDescription(peerConnection, answer);
        callback(answer);
      },
      function() { failure_('createAnswer'); },
      constraints);
}

function receiveAnswer(peerConnection, answer, callback) {
  var sessionDescription = new RTCSessionDescription(answer);
  peerConnection.setRemoteDescription(
      sessionDescription,
      function() {
        success_('setRemoteDescription');
        callback();
      },
      function() { failure_('setRemoteDescription'); });
}

function createPeerConnection(stun_server, useRtpDataChannels) {
  servers = {iceServers: [{url: 'stun:' + stun_server}]};
  try {
    var constraints = { optional: [{ RtpDataChannels: useRtpDataChannels }]};
    peerConnection = new RTCPeerConnection(servers, constraints);
  } catch (exception) {
    throw failTest('Failed to create peer connection: ' + exception);
  }
  peerConnection.onaddstream = addStreamCallback_;
  peerConnection.onremovestream = removeStreamCallback_;
  peerConnection.onicecandidate = iceCallback_;
  return peerConnection;
}

function createOffer(peerConnection, constraints, callback) {
  peerConnection.createOffer(
      function(localDescription) {
        success_('createOffer');
        setLocalDescription(peerConnection, localDescription);
        callback(localDescription);
      },
      function() { failure_('createOffer'); },
      constraints);
}

function hasSeenCryptoInSdp() {
  returnToTest(gHasSeenCryptoInSdp);
}

// Internals.
/** @private */
function success_(method) {
  debug(method + '(): success.');
}

/** @private */
function failure_(method, error) {
  throw failTest(method + '() failed: ' + error);
}

/** @private */
function iceCallback_(event) {
  if (event.candidate)
    sendIceCandidate(event.candidate);
}

/** @private */
function setLocalDescription(peerConnection, sessionDescription) {
  if (sessionDescription.sdp.search('a=crypto') != -1 ||
      sessionDescription.sdp.search('a=fingerprint') != -1)
    gHasSeenCryptoInSdp = 'crypto-seen';

  peerConnection.setLocalDescription(
    sessionDescription,
    function() { success_('setLocalDescription'); },
    function() { failure_('setLocalDescription'); });
}

/** @private */
function addStreamCallback_(event) {
  debug('Receiving remote stream...');
  var videoTag = document.getElementById('remote-view');
  attachMediaStream(videoTag, event.stream);
}

/** @private */
function removeStreamCallback_(event) {
  debug('Call ended.');
  document.getElementById('remote-view').src = '';
}
