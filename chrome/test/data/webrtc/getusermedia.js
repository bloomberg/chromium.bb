/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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
 * This function asks permission to use the webcam and mic from the browser. It
 * will return ok-requested to PyAuto. This does not mean the request was
 * approved though. The test will then have to click past the dialog that
 * appears in Chrome, which will run either the OK or failed callback as a
 * a result. To see which callback was called, use obtainGetUserMediaResult().
 */
function getUserMedia(requestVideo, requestAudio) {
  if (!navigator.webkitGetUserMedia) {
    returnToPyAuto('Browser does not support WebRTC.');
    return;
  }

  debug("Requesting webcam and microphone.");
  navigator.webkitGetUserMedia({video: requestVideo, audio: requestAudio},
                               getUserMediaOkCallback,
                               getUserMediaFailedCallback);
  returnToPyAuto('ok-requested');
}

/**
 * Must be called after calling getUserMedia. Returns not-called-yet if we have
 * not yet been called back by WebRTC. Otherwise it returns either ok-got-stream
 * or failed-with-error-x (where x is the error code from the error callback)
 * depending on which callback got called by WebRTC.
 */
function obtainGetUserMediaResult() {
  returnToPyAuto(gRequestWebcamAndMicrophoneResult);
}


/**
 * @return {string} Returns either the string ok-no-errors or a text message
 *     which describes the failure(s) which have been registered through calls
 *     to addTestFailure in this source file.
 */
function getAnyTestFailures() {
  if (gFailures.length == 1)
    returnToPyAuto('Test failure: ' + gFailures[0]);
  else if (gFailures.length > 1)
    returnToPyAuto('Multiple failures: ' + gFailures.join(' AND '));
  else
    returnToPyAuto('ok-no-errors');
}

// Internals.

function getUserMediaOkCallback(stream) {
  gLocalStream = stream;
  var streamUrl = webkitURL.createObjectURL(stream);
  document.getElementById("local_view").src = streamUrl;

  gRequestWebcamAndMicrophoneResult = 'ok-got-stream';
}

function getUserMediaFailedCallback(error) {
  gRequestWebcamAndMicrophoneResult = 'failed-with-error-' + error.code;
}