/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This file requires the functions defined in test_functions.js.

var gFingerprints = [];

// Public interface.

/**
 * Starts capturing frames from a video tag. The algorithm will fingerprint a
 * a frame every now and then. After calling this function and running for a
 * while (at least 200 ms) you will be able to call isVideoPlaying to see if
 * we detected any video.
 *
 * @param {string} videoElementId The video element to analyze.
 * @param {string} canvasId A canvas element to write fingerprints into.
 * @param {int}    width The video element's width.
 * @param {int}    width The video element's height.
 *
 * @return {string} Returns ok-started to PyAuto.
 */
//
function startDetection(videoElementId, canvasId, width, height) {
  var video = document.getElementById(videoElementId);
  var canvas = document.getElementById(canvasId);
  var NUM_FINGERPRINTS_TO_SAVE = 5;

  setInterval(function() {
    var context = canvas.getContext('2d');
    captureFrame_(video, context, width, height);
    gFingerprints.push(fingerprint_(context, width, height));
    if (gFingerprints.length > NUM_FINGERPRINTS_TO_SAVE) {
      gFingerprints.shift();
    }
  }, 100);

  returnToTest('ok-started');
}

/**
 * Checks if we have detected any video so far.
 *
 * @return {string} video-playing if we detected video, otherwise
 *                  video-not-playing.
 */
function isVideoPlaying() {
  // Video is considered to be playing if at least one finger print has changed
  // since the oldest fingerprint. Even small blips in the pixel data will cause
  // this check to pass. We only check for rough equality though to account for
  // rounding errors.
  try {
    if (gFingerprints.length > 1) {
      if (!allElementsRoughlyEqualTo_(gFingerprints, gFingerprints[0])) {
        returnToTest('video-playing');
        return;
      }
    }
  } catch (exception) {
    throw failTest('Failed to detect video: ' + exception.message);
  }
  returnToTest('video-not-playing');
}

// Internals.

/** @private */
function allElementsRoughlyEqualTo_(elements, element_to_compare) {
  if (elements.length == 0)
    return false;
  for (var i = 0; i < elements.length; i++) {
    if (Math.abs(elements[i] - element_to_compare) > 3) {
      return false;
    }
  }
  return true;
}

/** @private */
function captureFrame_(video, canvasContext, width, height) {
  canvasContext.drawImage(video, 0, 0, width, height);
}

/** @private */
function fingerprint_(canvasContext, width, height) {
  var imageData = canvasContext.getImageData(0, 0, width, height);
  var pixels = imageData.data;

  var fingerprint = 0;
  for (var i = 0; i < pixels.length; i++) {
    fingerprint += pixels[i];
  }
  return fingerprint;
}