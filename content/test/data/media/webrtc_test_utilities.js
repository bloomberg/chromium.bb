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

// Tells the C++ code we succeeded, which will generally exit the test.
function reportTestSuccess() {
  console.log('Test Success');
  window.domAutomationController.send('OK');
}

// Returns a custom return value to the test.
function sendValueToTest(value) {
  window.domAutomationController.send(value);
}

// Immediately fails the test on the C++ side.
function failTest(reason) {
  var error = new Error(reason);
  window.domAutomationController.send(error.stack);
}

// Called if getUserMedia fails.
function printGetUserMediaError(error) {
  var message = 'getUserMedia request unexpectedly failed:';
  if (error.constraintName)
    message += ' could not satisfy constraint ' + error.constraintName;
  else
    message += ' devices not working/user denied access.';
  failTest(message);
}

function detectVideoPlaying(videoElementName, callback) {
  detectVideo(videoElementName, isVideoPlaying, callback);
}

function detectVideoStopped(videoElementName, callback) {
  detectVideo(videoElementName,
              function (pixels, previous_pixels) {
                return !isVideoPlaying(pixels, previous_pixels);
              },
              callback);
}

function detectBlackVideo(videoElementName, callback) {
  detectVideo(videoElementName,
              function (pixels, previous_pixels) {
                return isVideoBlack(pixels);
              },
              callback);
}

function detectVideo(videoElementName, predicate, callback) {
  console.log('Looking at video in element ' + videoElementName);

  var width = VIDEO_TAG_WIDTH;
  var height = VIDEO_TAG_HEIGHT;
  var videoElement = $(videoElementName);
  var oldPixels = [];
  var startTimeMs = new Date().getTime();
  var waitVideo = setInterval(function() {
    var canvas = $(videoElementName + '-canvas');
    if (canvas == null) {
      console.log('Waiting for ' + videoElementName + '-canvas' + ' to appear');
      return;
    }
    var context = canvas.getContext('2d');
    context.drawImage(videoElement, 0, 0, width, height);
    var pixels = context.getImageData(0, 0 , width, height / 3).data;
    // Check that there is an old and a new picture with the same size to
    // compare and use the function |predicate| to detect the video state in
    // that case.
    // There's a failure(?) mode here where the video generated claims to
    // have size 2x2. Don't consider that a valid video.
    if (oldPixels.length == pixels.length &&
        predicate(pixels, oldPixels)) {
      console.log('Done looking at video in element ' + videoElementName);
      console.log('DEBUG: video.width = ' + videoElement.videoWidth);
      console.log('DEBUG: video.height = ' + videoElement.videoHeight);
      clearInterval(waitVideo);
      callback(videoElement.videoWidth, videoElement.videoHeight);
    }
    oldPixels = pixels;
    var elapsedTime = new Date().getTime() - startTimeMs;
    if (elapsedTime > 3000) {
      startTimeMs = new Date().getTime();
      console.log('Still waiting for video to satisfy ' + predicate.toString());
      console.log('DEBUG: video.width = ' + videoElement.videoWidth);
      console.log('DEBUG: video.height = ' + videoElement.videoHeight);
      // clearInterval(waitVideo);
      // callback(0, 0);
    }
  }, 200);
}

function waitForVideoWithResolution(element, expected_width, expected_height) {
  addExpectedEvent();
  detectVideoPlaying(element,
      function (width, height) {
        assertEquals(expected_width, width);
        assertEquals(expected_height, height);
        eventOccured();
      });
}

function waitForVideo(videoElement) {
  addExpectedEvent();
  detectVideoPlaying(videoElement, function () { eventOccured(); });
}

function waitForVideoToStop(videoElement) {
  addExpectedEvent();
  detectVideoStopped(videoElement, function () { eventOccured(); });
}

function waitForBlackVideo(videoElement) {
  addExpectedEvent();
  detectBlackVideo(videoElement, function () { eventOccured(); });
}

// Calculates the current frame rate and compares to |expected_frame_rate|
// |callback| is triggered with value |true| if the calculated frame rate
// is +-1 the expected or |false| if five calculations fail to match
// |expected_frame_rate|. Calls back with OK if the check passed, otherwise
// an error message.
function validateFrameRate(videoElementName, expected_frame_rate, callback) {
  var videoElement = $(videoElementName);
  var startTime = new Date().getTime();
  var decodedFrames = videoElement.webkitDecodedFrameCount;
  var attempts = 0;

  if (videoElement.readyState <= HTMLMediaElement.HAVE_CURRENT_DATA ||
          videoElement.paused || videoElement.ended) {
    failTest("getFrameRate - " + videoElementName + " is not plaing.");
    return;
  }

  var waitVideo = setInterval(function() {
    attempts++;
    currentTime = new Date().getTime();
    deltaTime = (currentTime - startTime) / 1000;
    startTime = currentTime;

    // Calculate decoded frames per sec.
    var fps =
        (videoElement.webkitDecodedFrameCount - decodedFrames) / deltaTime;
    decodedFrames = videoElement.webkitDecodedFrameCount;

    console.log('FrameRate in ' + videoElementName + ' is ' + fps);
    if (fps < expected_frame_rate + 1  && fps > expected_frame_rate - 1) {
      clearInterval(waitVideo);
      callback('OK');
    } else if (attempts == 5) {
      clearInterval(waitVideo);
      callback('Expected frame rate ' + expected_frame_rate + ' for ' +
               'element ' + videoElementName + ', but got ' + fps);
    }
  }, 1000);
}

function waitForConnectionToStabilize(peerConnection, callback) {
  peerConnection.onsignalingstatechange = function(event) {
    if (peerConnection.signalingState == 'stable') {
      peerConnection.onsignalingstatechange = null;
      callback();
    }
  }
}

// Adds an expected event. You may call this function many times to add more
// expected events. Each expected event must later be matched by a call to
// eventOccurred. When enough events have occurred, the "all events occurred
// handler" will be called.
function addExpectedEvent() {
  ++gNumberOfExpectedEvents;
}

// See addExpectedEvent.
function eventOccured() {
  ++gNumberOfEvents;
  if (gNumberOfEvents == gNumberOfExpectedEvents) {
    gAllEventsOccured();
  }
}

// This very basic video verification algorithm will be satisfied if any
// pixels are changed.
function isVideoPlaying(pixels, previousPixels) {
  for (var i = 0; i < pixels.length; i++) {
    if (pixels[i] != previousPixels[i]) {
      return true;
    }
  }
  return false;
}

// Checks if the frame is black. |pixels| is in RGBA (i.e. pixels[0] is the R
// value for the first pixel).
function isVideoBlack(pixels) {
  var threshold = 20;
  var accumulatedLuma = 0;
  for (var i = 0; i < pixels.length; i += 4) {
    // Ignore the alpha channel.
    accumulatedLuma += rec702Luma_(pixels[i], pixels[i + 1], pixels[i + 2]);
    if (accumulatedLuma > threshold * (i / 4 + 1))
      return false;
  }
  return true;
}

// Checks if the given color is within 1 value away from COLOR_BACKGROUND_GREEN.
function isAlmostBackgroundGreen(color) {
  if (Math.abs(color - COLOR_BACKGROUND_GREEN) > 1)
    return false;
  return true;
}

// Use Luma as in Rec. 709: Y′709 = 0.2126R + 0.7152G + 0.0722B;
// See http://en.wikipedia.org/wiki/Rec._709.
function rec702Luma_(r, g, b) {
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

// This function matches |left| and |right| and fails the test if the
// values don't match using normal javascript equality (i.e. the hard
// types of the operands aren't checked).
function assertEquals(expected, actual) {
  if (actual != expected) {
    failTest("expected '" + expected + "', got '" + actual + "'.");
  }
}

function assertNotEquals(expected, actual) {
  if (actual === expected) {
    failTest("expected '" + expected + "', got '" + actual + "'.");
  }
}

function assertTrue(booleanExpression, description) {
  if (!booleanExpression) {
    failTest(description);
  }
}

