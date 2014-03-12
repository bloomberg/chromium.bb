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
  window.domAutomationController.send('OK');
}

// Returns a custom return value to the test.
function sendValueToTest(value) {
  window.domAutomationController.send(value);
}

// Immediately fails the test on the C++ side and throw an exception to
// stop execution on the javascript side.
function failTest(reason) {
  var error = new Error(reason);
  window.domAutomationController.send(error.stack);
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

function detectVideo(videoElementName, predicate, callback) {
  console.log('Looking at video in element ' + videoElementName);

  var width = VIDEO_TAG_WIDTH;
  var height = VIDEO_TAG_HEIGHT;
  var videoElement = $(videoElementName);
  var canvas = $(videoElementName + '-canvas');
  var oldPixels = [];
  var waitVideo = setInterval(function() {
    var context = canvas.getContext('2d');
    context.drawImage(videoElement, 0, 0, width, height);
    var pixels = context.getImageData(0, 0 , width, height / 3).data;
    // Check that there is an old and a new picture with the same size to
    // compare and use the function |predicate| to detect the video state in
    // that case.
    if (oldPixels.length == pixels.length &&
        predicate(pixels, oldPixels)) {
      console.log('Done looking at video in element ' + videoElementName);
      clearInterval(waitVideo);
      callback();
    }
    oldPixels = pixels;
  }, 200);
}

function waitForVideo(videoElement) {
  addExpectedEvent();
  detectVideoPlaying(videoElement, function () { eventOccured(); });
}

function waitForVideoToStop(videoElement) {
  addExpectedEvent();
  detectVideoStopped(videoElement, function () { eventOccured(); });
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

// This function matches |left| and |right| and fails the test if the
// values don't match using normal javascript equality (i.e. the hard
// types of the operands aren't checked).
function assertEquals(expected, actual) {
  if (actual != expected) {
    failTest("expected '" + expected + "', got '" + actual + "'.");
  }
}
