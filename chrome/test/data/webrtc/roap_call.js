/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Public interface towards the other javascript files, such as
// message_handling.js. The contract for these functions is described in
// message_handling.js.

function handleMessage(peerConnection, message) {
  peerConnection.processSignalingMessage(message);
}

function createPeerConnection(stun_server) {
  peerConnection = new webkitDeprecatedPeerConnection(
      stun_server, signalingMessageCallback);
  peerConnection.onaddstream = addStreamCallback;
  peerConnection.onremovestream = removeStreamCallback;

  return peerConnection;
}

function setupCall(peerConnection) {
  peerConnection.addStream(gLocalStream);
}

function answerCall(peerConnection, message) {
  peerConnection.addStream(gLocalStream);
  handleMessage(peerConnection, message);
}

// Internals.
function signalingMessageCallback(message) {
  sendToPeer(gRemotePeerId, message);
}

function addStreamCallback(event) {
  debug('Receiving remote stream...');
  var streamUrl = webkitURL.createObjectURL(event.stream);
  document.getElementById('remote_view').src = streamUrl;

  // This means the call has been set up.
  returnToPyAuto('ok-call-established');
}

function removeStreamCallback(event) {
  debug('Call ended.');
  document.getElementById("remote_view").src = '';
}


