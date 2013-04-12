// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These must match with how the video and canvas tags are declared in html.
const VIDEO_TAG_WIDTH = 320;
const VIDEO_TAG_HEIGHT = 240;

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