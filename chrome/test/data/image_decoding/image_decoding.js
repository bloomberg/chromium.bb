// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Run benchmark for at least this many milliseconds.
var minTotalTimeMs = 10 * 1000;
// Run benchmark at least this many times.
var minIterations = 20;
// Discard results for this many milliseconds after startup.
var warmUpTimeMs = 5 * 1000;
var benchmarkStartTime;
var loadStartTimeMs;
var decodingTimesMs = [];
var isDone = false;
var img;

var now = (function() {
  if (window.performance)
    return (window.performance.now ||
            window.performance.webkitNow).bind(window.performance);
  return Date.now.bind(window);
})();

getImageFormat = function() {
  if (document.location.search) {
    return document.location.search.substr(1);
  }
  return "jpg";
}

prepareImageElement = function() {
  img = document.createElement('img');
  // Scale the image down to the display size to make sure the entire image is
  // decoded. If the image is much larger than the display, some browsers might
  // only decode the visible portion of the image, which would skew the results
  // of this test.
  img.setAttribute('width', '100%');
  document.body.appendChild(img);
  console.log("Running benchmark for at least " + minTotalTimeMs +
              " ms and at least " + minIterations + " times.");
  document.getElementById('status').innerHTML = "Benchmark running.";
  setStatus(getImageFormat().toUpperCase() + " benchmark running.");
  benchmarkStartTimeMs = now();
}

setStatus = function(status) {
  document.getElementById('status').innerHTML = status;
}

runBenchmark = function() {
  setStatus("Preparing benchmark.");
  prepareImageElement();
  startLoadingImage();
}

var iteration = (new Date).getTime();

startLoadingImage = function() {
  img.setAttribute('onload', '');
  img.setAttribute('src', '');
  loadStartTimeMs = now();
  img.addEventListener('load', onImageLoaded);
  // Use a unique URL for each test iteration to work around image caching.
  img.setAttribute('src', 'droids.' + getImageFormat() + '?' + iteration);
  iteration += 1;
}

var requestAnimationFrame = (function() {
  return window.requestAnimationFrame       ||
         window.webkitRequestAnimationFrame ||
         window.mozRequestAnimationFrame    ||
         window.oRequestAnimationFrame      ||
         window.msRequestAnimationFrame     ||
         function(callback) {
           window.setTimeout(callback, 1000 / 60);
         };
})().bind(window);

onImageLoaded = function(img) {
  var nowMs = now();
  var decodingTimeMs = nowMs - loadStartTimeMs;
  if (nowMs - benchmarkStartTimeMs >= warmUpTimeMs) {
    decodingTimesMs.push(decodingTimeMs);
  }
  if (nowMs - benchmarkStartTimeMs < minTotalTimeMs ||
      decodingTimesMs.length < minIterations) {
    requestAnimationFrame(startLoadingImage);
  } else {
    isDone = true;
    console.log("decodingTimes: " + decodingTimesMs);
    setStatus(getImageFormat().toUpperCase() + " benchmark finished: " +
              decodingTimesMs);
  }
}
