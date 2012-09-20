/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// The *Here functions are called from peerconnection.html and will make calls
// into our underlying javascript library with the values from the page.

function getUserMediaFromHere() {
  var audio = document.getElementById('audio').checked;
  var video = document.getElementById('video').checked;
  var hints = document.getElementById('media-hints').value;

  try {
    getUserMedia(video, audio, hints);
  } catch (exception) {
    print_('getUserMedia says: ' + exception);
  }
}

function connectFromHere() {
  var server = document.getElementById('server').value;
  // Generate a random name to distinguish us from other tabs:
  var name = 'peer_' + Math.floor(Math.random() * 10000);
  debug('Our name from now on will be ' + name);
  connect(server, name);
}

function callFromHere() {
  call();
}

function sendLocalStreamFromHere() {
  sendLocalStreamOverPeerConnection();
}

function removeLocalStreamFromHere() {
  removeLocalStream();
}

function hangUpFromHere() {
  hangUp();
  acceptIncomingCallsAgain();
}

function toggleRemoteFromHere() {
  toggleRemoteStream();
}

function toggleLocalFromHere() {
  toggleLocalStream();
}

function stopLocalFromHere() {
  stopLocalStream();
}

function showServerHelp() {
  alert('You need to build and run a peerconnection_server on some '
    + 'suitable machine. To build it in chrome, just run make/ninja '
    + 'peerconnection_server. Otherwise, read in https://code.google'
    + '.com/searchframe#xSWYf0NTG_Q/trunk/peerconnection/README&q=REA'
    + 'DME%20package:webrtc%5C.googlecode%5C.com.');
}

function clearLog() {
  document.getElementById('messages').innerHTML = '';
  document.getElementById('debug').innerHTML = '';
}

window.onload = function() {
  replaceReturnCallback(print_);
  replaceDebugCallback(debug_);
}

window.onunload = function() {
  if (!isDisconnected())
    disconnect();
}

// Internals.

/** @private */
function disabled_(element) {
  return document.getElementById(element).disabled;
}

/** @private */
function print_(message) {
  // Filter out uninteresting noise.
  if (message == 'ok-no-errors')
    return;

  console.log(message);
  document.getElementById('messages').innerHTML += message + '<br>';
}

/** @private */
function debug_(message) {
  console.log(message);
  document.getElementById('debug').innerHTML += message + '<br>';
}