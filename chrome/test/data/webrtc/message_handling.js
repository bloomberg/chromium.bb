/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This file requires these functions to be defined globally by someone else:
// function handleMessage(peerConnection, message)
// function createPeerConnection(stun_server)
// function setupCall(peerConnection)
// function answerCall(peerConnection, message)

// Currently these functions are supplied by jsep_call.js.

/**
 * This object represents the call.
 * @private
 */
var gPeerConnection = null;

/**
 * True if we are accepting incoming calls.
 * @private
 */
var gAcceptsIncomingCalls = true;

/**
 * Our peer id as assigned by the peerconnection_server.
 * @private
 */
var gOurPeerId = null;

/**
 * The client id we use to identify to peerconnection_server.
 * @private
 */
var gOurClientName = null;

/**
 * The URL to the peerconnection_server.
 * @private
 */
var gServerUrl = null;

/**
 * The remote peer's id. We receive this one either when we connect (in the case
 * our peer connects before us) or in a notification later.
 * @private
 */
var gRemotePeerId = null;

/**
 * We need a STUN server for some API calls.
 * @private
 */
var STUN_SERVER = 'stun.l.google.com:19302';

// Public interface to PyAuto test.


/**
 * Connects to the provided peerconnection_server.
 *
 * @param {string} serverUrl The server URL in string form without an ending
 *     slash, something like http://localhost:8888.
 * @param {string} clientName The name to use when connecting to the server.
 */
function connect(serverUrl, clientName) {
  if (gOurPeerId != null)
    failTest('connecting when already connected');

  debug('Connecting to ' + serverUrl + ' as ' + clientName);
  gServerUrl = serverUrl;
  gOurClientName = clientName;

  request = new XMLHttpRequest();
  request.open('GET', serverUrl + '/sign_in?' + clientName, true);
  debug(serverUrl + '/sign_in?' + clientName);
  request.onreadystatechange = function() {
    connectCallback_(request);
  }
  request.send();
}

/**
 * Initiates a call. We must be connected first, and we must have gotten hold
 * of a peer as well (e.g. precisely one peer must also have connected to the
 * server at this point).
 */
function call() {
  if (gOurPeerId == null)
    failTest('calling, but not connected');
  if (gRemotePeerId == null)
    failTest('calling, but missing remote peer');
  if (gPeerConnection != null)
    failTest('calling, but call is up already');

  gPeerConnection = createPeerConnection(STUN_SERVER);
  setupCall(gPeerConnection);
}

/**
 * Queries if a call is up or not. Returns either 'yes' or 'no' to PyAuto and
 * true to any calling javascript functions if the call is active.
 */
function isCallActive() {
  if (gPeerConnection == null)
    returnToTest('no');
  else
    returnToTest('yes');

  return gPeerConnection != null;
}

/**
 * Toggles the remote streams' enabled state on the peer connection, given that
 * a call is active. Returns ok-toggled on success.
 */
function toggleRemoteStream() {
  toggle_(gPeerConnection.remoteStreams[0]);
  returnToTest('ok-toggled');
}

/**
 * Toggles the local streams' enabled state on the peer connection, given that
 * a call is active. Returns ok-toggled on success.
 */
function toggleLocalStream() {
  toggle_(gPeerConnection.localStreams[0]);
  returnToTest('ok-toggled');
}

/**
 * Hangs up a started call. Returns ok-call-hung-up on success. This tab will
 * not accept any incoming calls after this call.
 */
function hangUp() {
  if (gPeerConnection == null)
    failTest('hanging up, but no call is active');
  sendToPeer(gRemotePeerId, 'BYE');
  closeCall_();
  gAcceptsIncomingCalls = false;
  returnToTest('ok-call-hung-up');
}

/**
 * Disconnects from the peerconnection server. Returns ok-disconnected on
 * success.
 */
function disconnect() {
  if (gOurPeerId == null)
    failTest('Disconnecting, but we are not connected.');

  request = new XMLHttpRequest();
  request.open('GET', gServerUrl + '/sign_out?peer_id=' + gOurPeerId, false);
  request.send();
  gOurPeerId = null;
  returnToTest('ok-disconnected');
}

// Public interface to signaling implementations, such as JSEP.

/**
 * Sends a message to a peer through the peerconnection_server.
 */
function sendToPeer(peer, message) {
  debug('Sending message ' + message + ' to peer ' + peer + '.');
  var request = new XMLHttpRequest();
  var url = gServerUrl + '/message?peer_id=' + gOurPeerId + '&to=' + peer;
  request.open('POST', url, false);
  request.setRequestHeader('Content-Type', 'text/plain');
  request.send(message);
}

// Internals.

/** @private */
function isDisconnected_() {
  return gOurPeerId == null;
}

/** @private */
function toggle_(stream) {
  if (!isCallActive())
    failTest('Cannot manipulate local streams: no call active.');

  stream.videoTracks[0].enabled = !stream.videoTracks[0].enabled;
  stream.audioTracks[0].enabled = !stream.audioTracks[0].enabled;
}

/** @private */
function connectCallback_(request) {
  debug('Connect callback: ' + request.status + ', ' + request.readyState);
  if (request.readyState == 4 && request.status == 200) {
    gOurPeerId = parseOurPeerId_(request.responseText);
    gRemotePeerId = parseRemotePeerIdIfConnected_(request.responseText);
    startHangingGet_(gServerUrl, gOurPeerId);
    returnToTest('ok-connected');
  }
}

/** @private */
function parseOurPeerId_(responseText) {
  // According to peerconnection_server's protocol.
  var peerList = responseText.split('\n');
  return parseInt(peerList[0].split(',')[1]);
}

/** @private */
function parseRemotePeerIdIfConnected_(responseText) {
  var peerList = responseText.split('\n');
  if (peerList.length == 1) {
    // No peers have connected yet - we'll get their id later in a notification.
    return null;
  }
  var remotePeerId = null;
  for (var i = 0; i < peerList.length; i++) {
    if (peerList[i].length == 0)
      continue;

    var parsed = peerList[i].split(',');
    var name = parsed[0];
    var id = parsed[1];

    if (id != gOurPeerId) {
      debug('Found remote peer with name ' + name + ', id ' +
        id + ' when connecting.');

      // There should be at most one remote peer in this test.
      if (remotePeerId != null)
        failTest('Expected just one remote peer in this test: found several.');

      // Found a remote peer.
      remotePeerId = id;
    }
  }
  return remotePeerId;
}

/** @private */
function startHangingGet_(server, ourId) {
  if (isDisconnected_())
    return;
  hangingGetRequest = new XMLHttpRequest();
  hangingGetRequest.onreadystatechange = function() {
    hangingGetCallback_(hangingGetRequest, server, ourId);
  }
  hangingGetRequest.ontimeout = function() {
    hangingGetTimeoutCallback_(hangingGetRequest, server, ourId);
  }
  callUrl = server + '/wait?peer_id=' + ourId;
  debug('Sending ' + callUrl);
  hangingGetRequest.open('GET', callUrl, true);
  hangingGetRequest.send();
}

/** @private */
function hangingGetCallback_(hangingGetRequest, server, ourId) {
  if (hangingGetRequest.readyState != 4)
    return;
  if (hangingGetRequest.status == 0) {
    // Code 0 is not possible if the server actually responded.
    failTest('Previous request was malformed, or server is unavailable.');
  }
  if (hangingGetRequest.status != 200) {
    failTest('Error ' + hangingGetRequest.status + ' from server: ' +
             hangingGetRequest.statusText);
  }
  var targetId = readResponseHeader_(hangingGetRequest, 'Pragma');
  if (targetId == ourId)
    handleServerNotification_(hangingGetRequest.responseText);
  else
    handlePeerMessage_(targetId, hangingGetRequest.responseText);

  hangingGetRequest.abort();
  restartHangingGet_(server, ourId);
}

/** @private */
function hangingGetTimeoutCallback_(hangingGetRequest, server, ourId) {
  debug('Hanging GET times out, re-issuing...');
  hangingGetRequest.abort();
  restartHangingGet_(server, ourId);
}

/** @private */
function handleServerNotification_(message) {
  var parsed = message.split(',');
  if (parseInt(parsed[2]) == 1) {
    // Peer connected - this must be our remote peer, and it must mean we
    // connected before them (except if we happened to connect to the server
    // at precisely the same moment).
    debug('Got remote peer from notification ' + parsed[0]);
    gRemotePeerId = parseInt(parsed[1]);
  }
}

/** @private */
function closeCall_() {
  if (gPeerConnection == null)
    failTest('Closing call, but no call active.');
  gPeerConnection.close();
  gPeerConnection = null;
}

/** @private */
function handlePeerMessage_(peerId, message) {
  debug('Received message from peer ' + peerId + ': ' + message);
  if (peerId != gRemotePeerId) {
    addTestFailure('Received notification from unknown peer ' + peerId +
                   ' (only know about ' + gRemotePeerId + '.');
    return;
  }
  if (message.search('BYE') == 0) {
    debug('Received BYE from peer: closing call');
    closeCall_();
    return;
  }
  if (gPeerConnection == null && gAcceptsIncomingCalls) {
    debug('We are being called: answer...');
    if (gLocalStream == null)
      failTest('We are being called, but we are not connected.');

    // The other side is calling us.
    gPeerConnection = createPeerConnection(STUN_SERVER);
    answerCall(gPeerConnection, message);
    return;
  }
  handleMessage(gPeerConnection, message);
}

/** @private */
function restartHangingGet_(server, ourId) {
  window.setTimeout(function() {
    startHangingGet_(server, ourId);
  }, 0);
}

/** @private */
function readResponseHeader_(request, key) {
  var value = request.getResponseHeader(key)
  if (value == null || value.length == 0) {
    addTestFailure('Received empty value ' + value +
                   ' for response header key ' + key + '.');
    return -1;
  }
  return parseInt(value);
}
