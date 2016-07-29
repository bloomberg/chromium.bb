/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * The duration of the all frame capture in milliseconds.
 * @private
 */
var gCaptureDuration = 0;

/**
 * The recorded video encoded in Base64.
 * @private
 */
var gVideoBase64 = '';

/**
 * Chunks of the video recorded by MediaRecorded as they become available.
 * @private
 */
var gChunks = [];

/**
 * A string to be returned to the test about the current status of capture.
 */
var gCapturingStatus = 'capturing-not-started';

/**
 * Starts the frame capturing.
 *
 * @param {!Object} The video tag from which the height and width parameters are
                    to be extracted.
 * @param {Number} The duration of the frame capture in seconds.
 */
function startFrameCapture(videoTag, duration) {
  debug('inputElement stream: ' + getStreamFromElement_(videoTag));
  var mediaRecorder = new MediaRecorder(getStreamFromElement_(videoTag));
  mediaRecorder.ondataavailable = function(recording) {
    gChunks.push(recording.data);
  }
  mediaRecorder.onstop = function() {
    var videoBlob = new Blob(gChunks, {type: "video/webm"});
    gChunks = [];
    var reader = new FileReader();
    reader.onloadend = function() {
      gVideoBase64 = reader.result.substr(reader.result.indexOf(',') + 1);
      gCapturingStatus = 'done-capturing';
      debug('done-capturing');
    }
    reader.readAsDataURL(videoBlob);
  }
  mediaRecorder.start();
  gCaptureDuration = 1000 * duration;
  setTimeout(function() { mediaRecorder.stop(); }, gCaptureDuration);
  gCapturingStatus = 'still-capturing';
}

/**
 * Returns the video recorded by RecordMedia encoded in Base64.
 */
function getRecordedVideoAsBase64() {
  silentReturnToTest(gVideoBase64);
}

/**
 * Queries if we're done with the frame capturing yet.
 */
function doneFrameCapturing() {
  returnToTest(gCapturingStatus);
}

/**
 * Returns the stream from the input element to be attached to MediaRecorder.
 * @private
 */
function getStreamFromElement_(element) {
  if (typeof element.srcObject !== 'undefined') {
    return element.srcObject;
  } else if (typeof element.mozSrcObject !== 'undefined') {
    return element.mozSrcObject;
  } else if (typeof element.src !== 'undefined') {
    return element.src;
  } else {
    console.log('Error attaching stream to element.');
  }
}
