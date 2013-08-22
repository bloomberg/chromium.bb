// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These must match with how the video and canvas tags are declared in html.
const VIDEO_TAG_WIDTH = 320;
const VIDEO_TAG_HEIGHT = 240;

// Fake video capture background green is of value 135.
const COLOR_BACKGROUND_GREEN = 135;

// Number of test events to occur before the test pass. When the test pass,
// the function gAllEventsOccured is called.
var gNumberOfExpectedEvents = 0;

// Number of events that currently have occurred.
var gNumberOfEvents = 0;

var gAllEventsOccured = function () {};

// Use this function to set a function that will be called once all expected
// events has occurred.
function setAllEventsOccuredHandler(handler) {
  gAllEventsOccured = handler;
}

function detectVideoIn(videoElementName, callback) {
  var width = VIDEO_TAG_WIDTH;
  var height = VIDEO_TAG_HEIGHT;
  var videoElement = $(videoElementName);
  var canvas = $(videoElementName + '-canvas');
  var waitVideo = setInterval(function() {
    var context = canvas.getContext('2d');
    context.drawImage(videoElement, 0, 0, width, height);
    var pixels = context.getImageData(0, 0, width, height).data;

    if (isVideoPlaying(pixels, width, height)) {
      clearInterval(waitVideo);
      callback();
    }
  }, 100);
}

function waitForVideo(videoElement) {
  document.title = 'Waiting for video...';
  addExpectedEvent();
  detectVideoIn(videoElement, function () { eventOccured(); });
}

function waitForConnectionToStabilize(peerConnection) {
  addExpectedEvent();
  var waitForStabilization = setInterval(function() {
    if (peerConnection.signalingState == 'stable') {
      clearInterval(waitForStabilization);
      eventOccured();
    }
  }, 100);
}

function addExpectedEvent() {
  ++gNumberOfExpectedEvents;
}

function eventOccured() {
  ++gNumberOfEvents;
  if (gNumberOfEvents == gNumberOfExpectedEvents) {
    gAllEventsOccured();
  }
}

// This very basic video verification algorithm will be satisfied if any
// pixels are nonzero in a small sample area in the middle. It relies on the
// assumption that a video element with null source just presents zeroes.
function isVideoPlaying(pixels, width, height) {
  // Sample somewhere near the middle of the image.
  var middle = width * height / 2;
  for (var i = 0; i < 20; i++) {
    if (pixels[middle + i] > 0) {
      return true;
    }
  }
  return false;
}

// This function matches |left| and |right| and throws an exception if the
// values don't match.
function expectEquals(left, right) {
  if (left != right) {
    var s = "expectEquals failed left: " + left + " right: " + right;
    document.title = s;
    throw s;
  }
}

// This function tries to calculate the aspect ratio shown by the fake capture
// device in the video tag. For this, we count the amount of light green pixels
// along |aperture| pixels on the positive X and Y axis starting from the
// center of the image. In this very center there should be a time-varying
// pacman; the algorithm counts for a couple of iterations and keeps the
// maximum amount of light green pixels on both directions. From this data
// the aspect ratio is calculated relative to a 320x240 window, so 4:3 would
// show as a 1. Furthermore, since an original non-4:3 might be letterboxed or
// cropped, the actual X and Y pixel amounts are compared with the fake video
// capture expected pacman radius (see further below).
function detectAspectRatio(callback) {
  var width = VIDEO_TAG_WIDTH;
  var height = VIDEO_TAG_HEIGHT;
  var videoElement = $('local-view');
  var canvas = $('local-view-canvas');

  var maxLightGreenPixelsX = 0;
  var maxLightGreenPixelsY = 0;

  var aperture = Math.min(width, height) / 2;
  var iterations = 0;
  var maxIterations = 10;

  var waitVideo = setInterval(function() {
    var context = canvas.getContext('2d');
    context.drawImage(videoElement, 0, 0, width, height);

    // We are interested in a window starting from the center of the image
    // where we expect the circle from the fake video capture to be rolling.
    var pixels =
        context.getImageData(width / 2, height / 2, aperture, aperture);

    var lightGreenPixelsX = 0;
    var lightGreenPixelsY = 0;

    // Walk horizontally counting light green pixels.
    for (var x = 0; x < aperture; ++x) {
      if (pixels.data[4 * x + 1] != COLOR_BACKGROUND_GREEN)
        lightGreenPixelsX++;
    }
    // Walk vertically counting light green pixels.
    for (var y = 0; y < aperture; ++y) {
      if (pixels.data[4 * y * aperture + 1] != 135)
        lightGreenPixelsY++;
    }
    if (lightGreenPixelsX > maxLightGreenPixelsX &&
        lightGreenPixelsX < aperture)
      maxLightGreenPixelsX = lightGreenPixelsX;
    if (lightGreenPixelsY > maxLightGreenPixelsY &&
        lightGreenPixelsY < aperture)
      maxLightGreenPixelsY = lightGreenPixelsY;

    var detectedAspectRatioString = "";
    if (++iterations > maxIterations) {
      clearInterval(waitVideo);
      observedAspectRatio = maxLightGreenPixelsY / maxLightGreenPixelsX;
      // At this point the observed aspect ratio is either 1, for undistorted
      // 4:3, or some other aspect ratio that is seen as distorted.
      if (Math.abs(observedAspectRatio - 1.333) < 0.1)
        detectedAspectRatioString = "16:9";
      else if (Math.abs(observedAspectRatio - 1.20) < 0.1)
        detectedAspectRatioString = "16:10";
      else if (Math.abs(observedAspectRatio - 1.0) < 0.1)
        detectedAspectRatioString = "4:3";
      else
        detectedAspectRatioString = "UNKNOWN aspect ratio";
      console.log(detectedAspectRatioString + " observed aspect ratio (" +
                  observedAspectRatio + ")");

      // The FakeVideoCapture calculates the circle radius as
      // std::min(capture_format_.width, capture_format_.height) / 4;
      // we do the same and see if both dimensions are scaled, meaning
      // we started from a cropped or stretched image.
      var nonDistortedRadius = Math.min(width, height) / 4;
      if ((maxLightGreenPixelsX != nonDistortedRadius) &&
          (maxLightGreenPixelsY != nonDistortedRadius)) {
        detectedAspectRatioString += " cropped";
      } else
        detectedAspectRatioString += " letterbox";

      console.log("Original image is: " + detectedAspectRatioString);
      callback(detectedAspectRatioString);
    }
  },
                              50);
}
