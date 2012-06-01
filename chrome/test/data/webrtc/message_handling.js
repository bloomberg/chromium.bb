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

// Currently these functions are supplied by roap_call.js or jsep_call.js.

/**
 * This object represents the call.
 * @private
 */
var gPeerConnection = null;

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
var STUN_SERVER = "STUN stun.l.google.com:19302";

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

  debug("Connecting to " + serverUrl + " as " + clientName);
  gServerUrl = serverUrl;
  gOurClientName = clientName;

  request = new XMLHttpRequest();
  request.open("GET", serverUrl + "/sign_in?" + clientName, true);
  debug(serverUrl + "/sign_in?" + clientName);
  request.onreadystatechange = function() {
    connectCallback(request);
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
 * Queries if a call is up or not. Returns either 'yes' or 'no' to PyAuto.
 */
function is_call_active() {
  if (gPeerConnection == null)
    returnToPyAuto('no');
  else
    returnToPyAuto('yes');
}

/**
 * Hangs up a started call. Returns ok-call-hung-up on success.
 */
function hangUp() {
  if (gPeerConnection == null)
    failTest('hanging up, but no call is active');
  sendToPeer(gRemotePeerId, 'BYE');
  closeCall();
  returnToPyAuto('ok-call-hung-up');
}

/**
 * Disconnects from the peerconnection server. Returns ok-disconnected on
 * success.
 */
function disconnect() {
  if (gOurPeerId == null)
    failTest('Disconnecting, but we are not connected.');

  request = new XMLHttpRequest();
  request.open("GET", gServerUrl + "/sign_out?peer_id=" + gOurPeerId, false);
  request.send();
  request = null;
  gOurPeerId = null;
  returnToPyAuto('ok-disconnected');
}

// Internals.

function connectCallback(request) {
  debug("Connect callback: " + request.status + ", " + request.readyState);
  if (request.readyState == 4 && request.status == 200) {
    gOurPeerId = parseOurPeerId(request.responseText);
    gRemotePeerId = parseRemotePeerIdIfConnected(request.responseText);
    startHangingGet(gServerUrl, gOurPeerId);
    returnToPyAuto('ok-connected');
  }
}

function parseOurPeerId(responseText) {
  // According to peerconnection_server's protocol.
  var peerList = responseText.split("\n");
  return parseInt(peerList[0].split(",")[1]);
}

function parseRemotePeerIdIfConnected(responseText) {
  var peerList = responseText.split("\n");
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
        failTest('Unexpected second peer');

      // Found a remote peer.
      remotePeerId = id;
    }
  }
  return remotePeerId;
}

function startHangingGet(server, ourId) {
  hangingGetRequest = new XMLHttpRequest();
  hangingGetRequest.onreadystatechange = function() {
    hangingGetCallback(hangingGetRequest, server, ourId);
  }
  hangingGetRequest.ontimeout = function() {
    hangingGetTimeoutCallback(hangingGetRequest, server, ourId);
  }
  callUrl = server + "/wait?peer_id=" + ourId;
  debug('Sending ' + callUrl);
  hangingGetRequest.open("GET", callUrl, true);
  hangingGetRequest.send();
}

function hangingGetCallback(hangingGetRequest, server, ourId) {
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
  var targetId = readResponseHeader(hangingGetRequest, 'Pragma');
  if (targetId == ourId)
    handleServerNotification(hangingGetRequest.responseText);
  else
    handlePeerMessage(targetId, hangingGetRequest.responseText);

  hangingGetRequest.abort();
  restartHangingGet(server, ourId);
}

function hangingGetTimeoutCallback(hangingGetRequest, server, ourId) {
  debug('Hanging GET times out, re-issuing...');
  hangingGetRequest.abort();
  restartHangingGet(server, ourId);
}

function sendToPeer(peer, message) {
  debug('Sending message ' + message + ' to peer ' + peer + '.');
  var request = new XMLHttpRequest();
  var url = gServerUrl + "/message?peer_id=" + gOurPeerId + "&to=" + peer;
  request.open("POST", url, false);
  request.setRequestHeader("Content-Type", "text/plain");
  request.send(message);
}

function handleServerNotification(message) {
  var parsed = message.split(",");
  if (parseInt(parsed[2]) == 1) {
    // Peer connected - this must be our remote peer, and it must mean we
    // connected before them (except if we happened to connect to the server
    // at precisely the same moment).
    debug('Got remote peer from notification ' + parsed[0]);
    gRemotePeerId = parseInt(parsed[1]);
  }
}

function closeCall() {
  if (gPeerConnection == null)
    failTest('Closing call, but no call active.');
  gPeerConnection.close();
  gPeerConnection = null;
}

function handlePeerMessage(peerId, message) {
  debug("Received message from peer " + peerId + ": " + message);
  if (peerId != gRemotePeerId) {
    addTestFailure('Received notification from unknown peer ' + peerId +
                   ' (only know about ' + gRemotePeerId + '.');
    return;
  }
  if (message.search('BYE') == 0) {
    debug("Received BYE from peer: closing call");
    closeCall();
    return;
  }
  if (gPeerConnection == null) {
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

function restartHangingGet(server, ourId) {
  window.setTimeout(function() {
    startHangingGet(server, ourId);
  }, 0);
}

function readResponseHeader(request, key) {
  var value = request.getResponseHeader(key)
  if (value == null || value.length == 0) {
    addTestFailure('Received empty value ' + value +
                   ' for response header key ' + key + '.');
    return -1;
  }
  return parseInt(value);
}
