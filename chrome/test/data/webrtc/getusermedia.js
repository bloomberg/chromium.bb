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
 * The MediaConstraints to use when connecting the local stream with a peer
 * connection.
 * @private
 */
var gAddStreamConstraints = {};

/**
 * String which keeps track of what happened when we requested user media.
 * @private
 */
var gRequestWebcamAndMicrophoneResult = 'not-called-yet';

/**
 * This function asks permission to use the webcam and mic from the browser. It
 * will return ok-requested to PyAuto. This does not mean the request was
 * approved though. The test will then have to click past the dialog that
 * appears in Chrome, which will run either the OK or failed callback as a
 * a result. To see which callback was called, use obtainGetUserMediaResult().
 *
 * @param {string} constraints Defines what to be requested, with mandatory
 *     and optional constraints defined. The contents of this parameter depends
 *     on the WebRTC version. This should be JavaScript code that we eval().
 */
function getUserMedia(constraints) {
  if (!navigator.webkitGetUserMedia) {
    returnToTest('Browser does not support WebRTC.');
    return;
  }
  try {
    var evaluatedConstraints;
    eval('evaluatedConstraints = ' + constraints);
  } catch (exception) {
    throw failTest('Not valid JavaScript expression: ' + constraints);
  }
  debug('Requesting getUserMedia: constraints: ' + constraints);
  navigator.webkitGetUserMedia(evaluatedConstraints,
                               getUserMediaOkCallback_,
                               getUserMediaFailedCallback_);
  returnToTest('ok-requested');
}

/**
 * Must be called after calling getUserMedia.
 * @return {string} Returns not-called-yet if we have not yet been called back
 *     by WebRTC. Otherwise it returns either ok-got-stream or
 *     failed-with-error-x (where x is the error code from the error
 *     callback) depending on which callback got called by WebRTC.
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
    throw failTest('Tried to stop local stream, ' +
                   'but media access is not granted.');

  gLocalStream.stop();
  returnToTest('ok-stopped');
}

// Functions callable from other JavaScript modules.

/**
 * Adds the current local media stream to a peer connection.
 * @param {RTCPeerConnection} peerConnection
 */
function addLocalStreamToPeerConnection(peerConnection) {
  if (gLocalStream == null)
    throw failTest('Tried to add local stream to peer connection, ' +
                   'but there is no stream yet.');
  try {
    peerConnection.addStream(gLocalStream, gAddStreamConstraints);
  } catch (exception) {
    throw failTest('Failed to add stream with constraints ' +
                   gAddStreamConstraints + ': ' + exception);
  }
  debug('Added local stream.');
}

/**
 * Removes the local stream from the peer connection.
 * @param {rtcpeerconnection} peerConnection
 */
function removeLocalStreamFromPeerConnection(peerConnection) {
  if (gLocalStream == null)
    throw failTest('Tried to remove local stream from peer connection, ' +
                   'but there is no stream yet.');
  try {
    peerConnection.removeStream(gLocalStream);
  } catch (exception) {
    throw failTest('Could not remove stream: ' + exception);
  }
  debug('Removed local stream.');
}

/**
 * @return {string} Returns the current local stream - |gLocalStream|.
 */
function getLocalStream() {
  return gLocalStream;
}

// Internals.

/**
 * @private
 * @param {MediaStream} stream Media stream.
 */
function getUserMediaOkCallback_(stream) {
  gLocalStream = stream;
  var videoTag = $('local-view');
  videoTag.src = webkitURL.createObjectURL(stream);

  // Due to crbug.com/110938 the size is 0 when onloadedmetadata fires.
  // videoTag.onloadedmetadata = displayVideoSize_(videoTag);.
  // Use setTimeout as a workaround for now.
  gRequestWebcamAndMicrophoneResult = 'ok-got-stream';
  setTimeout(function() {displayVideoSize_(videoTag);}, 500);
}

/**
 * @private
 * @param {string} videoTagId The ID of the video tag to update.
 * @param {string} width The width of the video to update the video tag, if
 *     width or height is 0, size will be taken from videoTag.videoWidth.
 * @param {string} height The height of the video to update the video tag, if
 *     width or height is 0 size will be taken from the videoTag.videoHeight.
 */
function updateVideoTagSize_(videoTagId, width, height) {
  var videoTag = $(videoTagId);
  if (width > 0 || height > 0) {
    videoTag.width = width;
    videoTag.height = height;
  }
  else {
    if (videoTag.videoWidth > 0 || videoTag.videoHeight > 0) {
      videoTag.width = videoTag.videoWidth;
      videoTag.height = videoTag.videoHeight;
    }
    else {
      return debug('"' + videoTagId + '" video stream size is 0, skipping ' +
                   'resize');
    }
  }
  debug('Set video tag "' + videoTagId + '" size to ' + videoTag.width + 'x' +
        videoTag.height);
  displayVideoSize_(videoTag);
}

/**
 * @private
 * @param {string} videoTag The ID of the video tag + stream to
 *     write the size to a HTML tag based on id.
 */
function displayVideoSize_(videoTag) {
  if (videoTag.videoWidth > 0 || videoTag.videoHeight > 0) {
    $(videoTag.id + '-stream-size').innerHTML = '(stream size: ' +
                                                videoTag.videoWidth + 'x' +
                                                videoTag.videoHeight + ')';
  }
  $(videoTag.id + '-size').innerHTML = videoTag.width + 'x' + videoTag.height;
}

/**
 * @private
 * @param {NavigatorUserMediaError} error Error containing details.
 */
function getUserMediaFailedCallback_(error) {
  debug('GetUserMedia FAILED: Maybe the camera is in use by another process?');
  gRequestWebcamAndMicrophoneResult = 'failed-with-error-' + error.code;
  debug(gRequestWebcamAndMicrophoneResult);
}

$ = function(id) {
  return document.getElementById(id);
};
