/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Public interface towards the other javascript files, such as
// message_handling.js. The contract for these functions is described in
// message_handling.js.

/**
 * See http://dev.w3.org/2011/webrtc/editor/webrtc.html for more information
 * on the JSEP standard.
 */

function handleMessage(peerConnection, message) {
  var parsed_msg = message.split('###');
  if (parsed_msg.length != 2) {
    addTestFailure('Failed to parse ' + message);
    return;
  }

  if (parsed_msg[0] == 'candidate') {
    var candidate_msg = parsed_msg[1].split('##');
    var candidate = new IceCandidate(candidate_msg[0], candidate_msg[1]);
    peerConnection.processIceMessage(candidate);
    return;
  }
  if (parsed_msg[0] == 'sdp answer') {
    var answer = new SessionDescription(parsed_msg[1]);
    peerConnection.setRemoteDescription(webkitPeerConnection00.SDP_ANSWER,
                                        answer);
    return;
  }
  if (parsed_msg[0] == 'sdp offer') {
    var offer = new SessionDescription(parsed_msg[1]);
    peerConnection.setRemoteDescription(webkitPeerConnection00.SDP_OFFER,
                                        offer);
    var answer = peerConnection.createAnswer(offer.toSdp(),
                                            {has_audio:true,has_video:true});
    peerConnection.setLocalDescription(webkitPeerConnection00.SDP_ANSWER,
                                       answer);
    var msg = 'sdp answer' + '###' + answer.toSdp();
    sendToPeer(gRemotePeerId, msg);
    return;
  }
  addTestFailure('unknown message received');
  return;
}

function createPeerConnection(stun_server) {
  peerConnection = new webkitPeerConnection00('STUN ' + stun_server,
                                              iceCallback_);
  peerConnection.onaddstream = addStreamCallback_;
  peerConnection.onremovestream = removeStreamCallback_;
  return peerConnection;
}

function setupCall(peerConnection) {
  peerConnection.addStream(gLocalStream);
  var offer = peerConnection.createOffer({has_audio:true,has_video:true});
  peerConnection.setLocalDescription(webkitPeerConnection00.SDP_OFFER, offer);
  peerConnection.startIce();
  var msg = 'sdp offer' + '###' + offer.toSdp();
  sendToPeer(gRemotePeerId, msg);
}

function answerCall(peerConnection, message) {
  peerConnection.addStream(gLocalStream);
  handleMessage(peerConnection,message);
  peerConnection.startIce();
}

// Internals.

/** @private */
function iceCallback_(candidate, more_to_follow) {
  if (candidate == null) {
    debug('Received end of candidates.');
    return;
  }
  var msg = 'candidate' + '###' + candidate.label + '##' + candidate.toSdp();
  sendToPeer(gRemotePeerId, msg);
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
  document.getElementById('remote-view').src = '';
}
