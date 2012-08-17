// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Inject this script on any page to measure framerate as the page is scrolled
// from top to bottom.
//
// Usage:
//   1. Define a callback that takes the results array as a parameter.
//   2. To start the test, call new __ScrollTest(callback).
//   3a. When the test is complete, the callback will be called.
//   3b. If no callback is specified, the results are sent to the console.

(function() {
  var getTimeMs = (function() {
    if (window.performance)
      return (performance.now       ||
              performance.mozNow    ||
              performance.msNow     ||
              performance.oNow      ||
              performance.webkitNow).bind(window.performance);
    else
      return function() { return new Date().getTime(); };
  })();

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

  function GpuBenchmarkingScrollStats() {
    this.initialStats_ = this.getRenderingStats_();
  }

  GpuBenchmarkingScrollStats.prototype.getResult = function() {
    var stats = this.getRenderingStats_();
    for (var key in stats)
      stats[key] -= this.initialStats_[key];
    return stats;
  };

  GpuBenchmarkingScrollStats.prototype.getRenderingStats_ = function() {
    var stats = chrome.gpuBenchmarking.renderingStats();
    stats.totalTimeInSeconds = getTimeMs() / 1000;
    return stats;
  };

  function RafScrollStats(timestamp) {
    this.frameTimes_ = [timestamp];
    this.recording_ = true;
    requestAnimationFrame(this.processStep_.bind(this));
  }

  RafScrollStats.prototype.getResult = function() {
    this.recording_ = false;

    // Fill in the result object.
    var result = {};
    result.numAnimationFrames = this.frameTimes_.length - 1;
    result.droppedFrameCount = this.getDroppedFrameCount_(this.frameTimes_);
    result.totalTimeInSeconds = (this.frameTimes_[this.frameTimes_.length - 1] -
        this.frameTimes_[0]) / 1000;
    return result;
  };

  RafScrollStats.prototype.processStep_ = function(timestamp) {
    if (!this.recording_)
      return;

    this.frameTimes_.push(timestamp);
    requestAnimationFrame(this.processStep_.bind(this));
  };

  RafScrollStats.prototype.getDroppedFrameCount_ = function(frameTimes) {
    var droppedFrameCount = 0;
    for (var i = 1; i < frameTimes.length; i++) {
      var frameTime = frameTimes[i] - frameTimes[i-1];
      if (frameTime > 1000 / 55)
        droppedFrameCount++;
    }
    return droppedFrameCount;
  };

  // In this class, a "step" is when the page is being scrolled.
  // i.e. startScroll -> startStep -> scroll -> processStep ->
  //                  -> startStep -> scroll -> processStep -> endScroll
  function ScrollTest(callback, opt_isGmailTest) {
    var self = this;

    this.TOTAL_ITERATIONS_ = 2;
    this.SCROLL_DELTA_ = 100;

    this.callback_ = callback;
    this.isGmailTest_ = opt_isGmailTest;
    this.iteration_ = 0;

    this.results_ = []

    if (this.isGmailTest_) {
      gmonkey.load('2.0', function(api) {
          self.start_(api.getScrollableElement());
      });
    } else {
      if (document.readyState == 'complete')
        this.start_();
      else
        window.addEventListener('load', function() { self.start_(); });
    }
  }

  ScrollTest.prototype.scroll_ = function(x, y) {
    if (this.isGmailTest_)
      this.element_.scrollByLines(1);
    else
      window.scrollBy(x, y);
  };

  ScrollTest.prototype.start_ = function(opt_element) {
    // Assign this.element_ here instead of constructor, because the constructor
    // ensures this method will be called after the document is loaded.
    this.element_ = opt_element || document.body;
    requestAnimationFrame(this.startScroll_.bind(this));
  };

  ScrollTest.prototype.startScroll_ = function(timestamp) {
    this.element_.scrollTop = 0;
    if (window.chrome && chrome.gpuBenchmarking)
      this.scrollStats_ = new GpuBenchmarkingScrollStats();
    else
      this.scrollStats_ = new RafScrollStats(timestamp);
    this.startStep_();
  };

  ScrollTest.prototype.startStep_ = function() {
    this.scroll_(0, this.SCROLL_DELTA_);
    requestAnimationFrame(this.processStep_.bind(this));
  };

  ScrollTest.prototype.endScroll_ = function() {
    this.results_.push(this.scrollStats_.getResult());
    this.iteration_++;
  };

  ScrollTest.prototype.processStep_ = function(timestamp) {
    // clientHeight is "special" for the body element.
    if (this.element_ == document.body)
      var clientHeight = window.innerHeight;
    else
      var clientHeight = this.element_.clientHeight;
    var isScrollComplete =
        this.element_.scrollTop + clientHeight >= this.element_.scrollHeight;

    if (!isScrollComplete) {
      this.startStep_();
      return;
    }

    this.endScroll_();

    var isTestComplete = this.iteration_ >= this.TOTAL_ITERATIONS_;
    if (!isTestComplete) {
      requestAnimationFrame(this.startScroll_.bind(this));
      return;
    }

    // Send results.
    if (this.callback_)
      this.callback_(this.results_);
    else
      console.log(this.results_);
  };

  window.__ScrollTest = ScrollTest;
})();
