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
    peerConnection.setRemoteDescription(session_description);
    if (session_description.type == "offer") {
      peerConnection.createAnswer(setLocalAndSendMessage_);
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
  servers = {iceServers:[{uri:"stun:" + stun_server}]};
  peerConnection = new webkitRTCPeerConnection(servers, null);
  peerConnection.onaddstream = addStreamCallback_;
  peerConnection.onremovestream = removeStreamCallback_;
  peerConnection.onicecandidate = iceCallback_;
  return peerConnection;
}

function setupCall(peerConnection) {
  peerConnection.addStream(gLocalStream);
  peerConnection.createOffer(setLocalAndSendMessage_);
}

function answerCall(peerConnection, message) {
  peerConnection.addStream(gLocalStream);
  handleMessage(peerConnection,message);
}

// Internals.
/** @private */
function iceCallback_(event) {
  if (event.candidate)
    sendToPeer(gRemotePeerId, JSON.stringify(event.candidate));
}

/** @private */
function setLocalAndSendMessage_(session_description) {
  peerConnection.setLocalDescription(session_description);
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
  returnToTest('ok-call-established');
}

/** @private */
function removeStreamCallback_(event) {
  debug('Call ended.');
  document.getElementById("remote-view").src = '';
}
