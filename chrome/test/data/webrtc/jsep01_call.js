/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

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
      peerConnection.createAnswer(
        setLocalAndSendMessage_,
        function() { failure_('createAnswer'); },
        getCurrentMediaHints());
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
    failTest('Failed to create peer connection: ' + exception);
  }
  peerConnection.onaddstream = addStreamCallback_;
  peerConnection.onremovestream = removeStreamCallback_;
  peerConnection.onicecandidate = iceCallback_;
  return peerConnection;
}

function setupCall(peerConnection) {
  peerConnection.createOffer(
      setLocalAndSendMessage_,
      function () { success_('createOffer'); },
      getCurrentMediaHints());
}

function answerCall(peerConnection, message) {
  handleMessage(peerConnection,message);
}

// Internals.
/** @private */
function success_(method) {
  debug(method + '(): success.');
}

/** @private */
function failure_(method, error) {
  failTest(method + '() failed: ' + error);
}

/** @private */
function iceCallback_(event) {
  if (event.candidate)
    sendToPeer(gRemotePeerId, JSON.stringify(event.candidate));
}

/** @private */
function setLocalAndSendMessage_(session_description) {
  peerConnection.setLocalDescription(
    session_description,
    function() { success_('setLocalDescription'); },
    function() { failure_('setLocalDescription'); });
  sendToPeer(gRemotePeerId, JSON.stringify(session_description));
}

/** @private */
function addStreamCallback_(event) {
  debug('Receiving remote stream...');
  var streamUrl = webkitURL.createObjectURL(event.stream);
  document.getElementById('remote-view').src = streamUrl;

  // This means the call has been set up.
  // This only mean that we have received a valid SDP message with an offer or
  // an answer, it does not mean that audio and video works.
  returnToTest('ok-got-remote-stream');
}

/** @private */
function removeStreamCallback_(event) {
  debug('Call ended.');
  document.getElementById("remote-view").src = '';
}
