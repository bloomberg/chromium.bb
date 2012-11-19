/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/** @private */
var gTransformOutgoingSdp = function(sdp) { return sdp; }

/** @private */
var gCreateAnswerConstraints = {};

/** @private */
var gCreateOfferConstraints = {};

/**
 * Sets the transform to apply just before setting the local description and
 * sending to the peer.
 * @param{function} transformFunction A function which takes one SDP string as
 *     argument and returns the modified SDP string.
 */
function setOutgoingSdpTransform(transformFunction) {
  gTransformOutgoingSdp = transformFunction;
}

/**
 * Sets the MediaConstraints to be used for PeerConnection createAnswer() calls.
 * @param{string} mediaConstraints The constraints, as defined in the
 *     PeerConnection JS API spec.
 */
function setCreateAnswerConstraints(mediaConstraints) {
  gCreateAnswerConstraints = mediaConstraints;
}

/**
 * Sets the MediaConstraints to be used for PeerConnection createOffer() calls.
 * @param{string} mediaConstraints The constraints, as defined in the
 *     PeerConnection JS API spec.
 */
function setCreateOfferConstraints(mediaConstraints) {
  gCreateOfferConstraints = mediaConstraints;
}


// Public interface towards the other javascript files, such as
// message_handling.js. The contract for these functions is described in
// message_handling.js.

function handleMessage(peerConnection, message) {
  var parsed_msg = JSON.parse(message);
  if (parsed_msg.type) {
    var session_description = new RTCSessionDescription(parsed_msg);
    peerConnection.setRemoteDescription(
        session_description,
        function() { success_('setRemoteDescription'); },
        function() { failure_('setRemoteDescription'); });
    if (session_description.type == "offer") {
      debug('createAnswer with constraints: ' +
            JSON.stringify(gCreateAnswerConstraints, null, ' '));
      peerConnection.createAnswer(
        setLocalAndSendMessage_,
        function() { failure_('createAnswer'); },
        gCreateAnswerConstraints);
    }
    return;
  } else if (parsed_msg.candidate) {
    var candidate = new RTCIceCandidate(parsed_msg);
    peerConnection.addIceCandidate(candidate);
    return;
  }
  addTestFailure("unknown message received");
  return;
}

function createPeerConnection(stun_server) {
  servers = {iceServers:[{url:"stun:" + stun_server}]};
  try {
    peerConnection = new webkitRTCPeerConnection(servers, null);
  } catch (exception) {
    throw failTest('Failed to create peer connection: ' + exception);
  }
  peerConnection.onaddstream = addStreamCallback_;
  peerConnection.onremovestream = removeStreamCallback_;
  peerConnection.onicecandidate = iceCallback_;
  return peerConnection;
}

function setupCall(peerConnection) {
  debug('createOffer with constraints: ' +
        JSON.stringify(gCreateOfferConstraints, null, ' '));
  peerConnection.createOffer(
      setLocalAndSendMessage_,
      function () { failure_('createOffer'); },
      gCreateOfferConstraints);
}

function answerCall(peerConnection, message) {
  handleMessage(peerConnection, message);
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
    sendToPeer(gRemotePeerId, JSON.stringify(event.candidate));
}

/** @private */
function setLocalAndSendMessage_(session_description) {
  session_description.sdp = gTransformOutgoingSdp(session_description.sdp);
  peerConnection.setLocalDescription(
    session_description,
    function() { success_('setLocalDescription'); },
    function() { failure_('setLocalDescription'); });
  debug("Sending SDP message:\n" + session_description.sdp);
  sendToPeer(gRemotePeerId, JSON.stringify(session_description));
}

/** @private */
function addStreamCallback_(event) {
  debug('Receiving remote stream...');
  var videoTag = document.getElementById('remote-view');
  videoTag.src = webkitURL.createObjectURL(event.stream);

  // Due to crbug.com/110938 the size is 0 when onloadedmetadata fires.
  // videoTag.onloadedmetadata = updateVideoTagSize_('remote-view');
  // Use setTimeout as a workaround for now.
  setTimeout(function() {updateVideoTagSize_('remote-view')}, 500);
}

/** @private */
function removeStreamCallback_(event) {
  debug('Call ended.');
  document.getElementById("remote-view").src = '';
}
