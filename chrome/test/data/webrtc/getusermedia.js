/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * See http://dev.w3.org/2011/webrtc/editor/getusermedia.html for more
 * information on getUserMedia.
 */

/**
 * Keeps track of our local stream (e.g. what our local webcam is streaming).
 * @private
 */
var gLocalStream = null;

/**
 * String which keeps track of what happened when we requested user media.
 * @private
 */
var gRequestWebcamAndMicrophoneResult = 'not-called-yet';

/**
 * The media hints to use when connecting the local stream with a peer
 * connection.
 * @private
 */
var gMediaHints = {};

/**
 * This function asks permission to use the webcam and mic from the browser. It
 * will return ok-requested to PyAuto. This does not mean the request was
 * approved though. The test will then have to click past the dialog that
 * appears in Chrome, which will run either the OK or failed callback as a
 * a result. To see which callback was called, use obtainGetUserMediaResult().
 *
 * @param requestVideo, requestAudio: whether to request video/audio.
 * @param mediaHints The media hints to use when we add streams to a peer
 *     connection later. The contents of this parameter depends on the WebRTC
 *     version. This should be javascript code that we eval().
 */
function getUserMedia(requestVideo, requestAudio, mediaHints) {
  if (!navigator.webkitGetUserMedia) {
    returnToTest('Browser does not support WebRTC.');
    return;
  }
  try {
    eval('gMediaHints = ' + mediaHints);
  } catch (exception) {
    failTest('Not valid javascript expression: ' + mediaHints);
  }

  debug('Requesting: video ' + requestVideo + ', audio ' + requestAudio);
  navigator.webkitGetUserMedia({video: requestVideo, audio: requestAudio},
                               getUserMediaOkCallback_,
                               getUserMediaFailedCallback_);
  returnToTest('ok-requested');
}

/**
 * Must be called after calling getUserMedia. Returns not-called-yet if we have
 * not yet been called back by WebRTC. Otherwise it returns either ok-got-stream
 * or failed-with-error-x (where x is the error code from the error callback)
 * depending on which callback got called by WebRTC.
 */
function obtainGetUserMediaResult() {
  returnToTest(gRequestWebcamAndMicrophoneResult);
  return gRequestWebcamAndMicrophoneResult;
}

/**
 * Stops the local stream.
 */
function stopLocalStream() {
  if (gLocalStream == null)
    failTest('Tried to stop local stream, but media access is not granted.');

  gLocalStream.stop();
  returnToTest('ok-stopped');
}

// Functions callable from other JavaScript modules.

/**
 * Returns the media hints as passed into getUserMedia.
 */
function getCurrentMediaHints() {
  return gMediaHints;
}

/**
 * Adds the current local media stream to a peer connection.
 */
function addLocalStreamToPeerConnection(peerConnection) {
  if (gLocalStream == null)
    failTest('Tried to add local stream to peer connection,'
        + ' but there is no stream yet.');
  try {
    peerConnection.addStream(gLocalStream, gMediaHints);
  } catch (exception) {
    failTest('Failed to add stream with hints ' + gMediaHints + ': '
        + exception);
  }
  debug('Added local stream.');
}

/**
 * Removes the local stream from the peer connection.
 * @param peerConnection
 */
function removeLocalStreamFromPeerConnection(peerConnection) {
  if (gLocalStream == null)
    failTest('Tried to remove local stream from peer connection,'
        + ' but there is no stream yet.');
  try {
    peerConnection.removeStream(gLocalStream);
  } catch (exception) {
    failTest('Could not remove stream: ' + exception);
  }
  debug('Removed local stream.');
}

// Internals.

/** @private */
function getUserMediaOkCallback_(stream) {
  gLocalStream = stream;
  var streamUrl = webkitURL.createObjectURL(stream);
  document.getElementById('local-view').src = streamUrl;

  gRequestWebcamAndMicrophoneResult = 'ok-got-stream';
}

/** @private */
function getUserMediaFailedCallback_(error) {
  gRequestWebcamAndMicrophoneResult = 'failed-with-error-' + error.code;
}