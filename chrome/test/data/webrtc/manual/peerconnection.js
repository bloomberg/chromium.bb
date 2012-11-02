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

function toggleRemoteVideoFromHere() {
  toggleRemoteStream(function(remoteStream) {
    return remoteStream.videoTracks[0];
  }, 'video');
}

function toggleRemoteAudioFromHere() {
  toggleRemoteStream(function(remoteStream) {
    return remoteStream.audioTracks[0];
  }, 'audio');
}

function toggleLocalVideoFromHere() {
  toggleLocalStream(function(localStream) {
    return localStream.videoTracks[0];
  }, 'video');
}

function toggleLocalAudioFromHere() {
  toggleLocalStream(function(localStream) {
    return localStream.audioTracks[0];
  }, 'audio');
}

function stopLocalFromHere() {
  stopLocalStream();
}

function forceOpusChanged() {
  var forceOpus = document.getElementById('force-opus').checked;
  if (forceOpus) {
    forceOpus_();
  } else {
    dontTouchSdp_();
  }
}

function showServerHelp() {
  alert('You need to build and run a peerconnection_server on some '
    + 'suitable machine. To build it in chrome, just run make/ninja '
    + 'peerconnection_server. Otherwise, read in https://code.google'
    + '.com/searchframe#xSWYf0NTG_Q/trunk/peerconnection/README&q=REA'
    + 'DME%20package:webrtc%5C.googlecode%5C.com.');
}

function toggleHelp() {
  var help = document.getElementById('help');
  if (help.style.display == 'none')
    help.style.display = 'inline';
  else
    help.style.display = 'none';
}

function clearLog() {
  document.getElementById('messages').innerHTML = '';
  document.getElementById('debug').innerHTML = '';
}

window.onload = function() {
  replaceReturnCallback(print_);
  replaceDebugCallback(debug_);
  doNotAutoAddLocalStreamWhenCalled();
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

/** @private */
function swapSdpLines_(sdp, line, swapWith) {
  var lines = sdp.split('\r\n');
  var lineStart = lines.indexOf(line);
  var swapLineStart = lines.indexOf(swapWith);
  if (lineStart == -1 || swapLineStart == -1)
    return sdp;  // This generally happens on the first message.

  var tmp = lines[lineStart];
  lines[lineStart] = lines[swapLineStart];
  lines[swapLineStart] = tmp;

  return lines.join('\r\n');
}

// TODO(phoglund): Not currently used; delete once it clear we do not need to
// test opus prioritization.
/** @private */
function preferOpus_() {
  setOutgoingSdpTransform(function(sdp) {
    sdp = sdp.replace('103 104 111', '111 103 104');

    // TODO(phoglund): We need to swap the a= lines too. I don't think this
    // should be needed but it apparently is right now.
    return swapSdpLines_(sdp,
                         "a=rtpmap:103 ISAC/16000",
                         "a=rtpmap:111 opus/48000");
  });
}

/** @private */
function forceOpus_() {
  setOutgoingSdpTransform(function(sdp) {
    // Remove all other codecs.
    sdp = sdp.replace(/m=audio (\d+) RTP\/SAVPF.*\r\n/g,
                      'm=audio $1 RTP/SAVPF 111\r\n');
    sdp = sdp.replace(/a=rtpmap:(?!111)\d{1,3}.*\r\n/g, '');
    return sdp;
  });
}

/** @private */
function dontTouchSdp_() {
  setOutgoingSdpTransform(function(sdp) { return sdp; });
}
