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

  try {
    getUserMedia(video, audio);
  } catch (exception) {
    print_('getUserMedia says: ' + exception);
  }
}

function connectFromHere() {
  if (obtainGetUserMediaResult() != 'ok-got-stream') {
    print_('<b>Grant media access first.</b>')
    return;
  }

  var server = document.getElementById('server').value;
  connect(server, 'whatever');  // Name doesn't matter

  disable_('connect');
  enable_('call');
}

function callFromHere() {
  call();
  disable_('connect');
  disable_('call');
  enable_('hangup');
  enable_('toggle-remote');
  enable_('toggle-local');
}

function hangUpFromHere() {
  hangUp();
  disable_('connect');
  enable_('call');
  disable_('hangup');
  disable_('toggle-remote');
  disable_('toggle-local');
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

window.onload = function() {
  replaceReturnCallback(print_);
  checkErrorsPeriodically_();
  getUserMedia(true, true);
}

window.onunload = function() {
  if (disabled_('connect'))
    disconnect();
}

// Internals.

/** @private */
function enable_(element) {
  document.getElementById(element).disabled = false;
}

/** @private */
function disable_(element) {
  document.getElementById(element).disabled = true;
}

/** @private */
function disabled_(element) {
  return document.getElementById(element).disabled;
}

/** @private */
function print_(message) {
  // Filter out uninteresting noise.
  if (message == 'ok-no-errors')
    return;

  debug(message);
  document.getElementById('debug').innerHTML += message + '<br>';
}

/** @private */
function checkErrorsPeriodically_() {
  setInterval(function() {
    getAnyTestFailures();
  }, 100);
}
