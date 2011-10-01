// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Function that sets a callback called when the next frame is rendered.
var __set_frame_callback;

// Amount of scrolling at each frame.
var __scroll_delta = 100;

// Current scroll position.
var __ypos;

// Time of previous scroll callback execution.
var __start_time;

// Warmup run indicator.
var __warm_up;

// True when all scrolling has completed.
var __scrolling_complete;

// Deltas are in milliseconds.
var __delta_max;
var __delta_min;
var __delta_sum;
var __delta_square_sum;
var __n_samples;

var __mean;  // Milliseconds between frames.
var __mean_fps;  // Frames per second.
var __variance;
var __sigma;


// Initializes the platform-independent frame callback scheduler function.
function __init_set_frame_callback() {
  if (!__set_frame_callback) {
    if ("webkitRequestAnimationFrame" in window)
      __set_frame_callback = webkitRequestAnimationFrame;
    else if ("mozRequestAnimationFrame" in window)
      __set_frame_callback = mozRequestAnimationFrame;
  }
}


// Collects statistical information for the time intervals between frames.
function record_delta_time() {
  var time = new Date().getTime();
  if (__start_time != 0) {
    var delta = time - __start_time;
    document.title = delta + " (" + Math.round(1000.0 / delta) + ")";
    __n_samples++;
    __delta_sum += delta;
    __delta_square_sum += delta * delta;
    if (delta < __delta_min) {
      __delta_min = delta;
    }
    if (delta > __delta_max) {
      __delta_max = delta;
    }
  }
  __start_time = time;
}


// Scrolls page down and reschedules itself until it hits the bottom.  Collects
// stats along the way.
function __frame_callback() {
  record_delta_time();
  window.scrollBy(0, __scroll_delta);
  __ypos += __scroll_delta;
  scrollable_distance = document.body.scrollHeight - window.innerHeight;
  if (__ypos < scrollable_distance) {
    __set_frame_callback(__frame_callback, document.body);
  } else if (__warm_up) {
    // The first pass is a warm up.  When done, start the scroll test again,
    // this time for real.
    __warm_up = false;
    __continue_scroll_test();
  } else {
    __compute_results();
    __scrolling_complete = true;
  }
}


// Positions document to start test.
function __first_frame_callback() {
  document.body.scrollTop = 0;
  __ypos = 0;
  __set_frame_callback(__frame_callback, document.body);
}


function __init_stats() {
  __delta_max = 0
  __delta_min = 1000000;
  __delta_sum = 0.0;
  __delta_square_sum = 0.0;
  __n_samples = 0;
}


function __continue_scroll_test() {
  __start_time = 0;
  __init_set_frame_callback();
  __init_stats();
  __set_frame_callback(__first_frame_callback, document.body);
}


// Performs the scroll test.
function __scroll_test() {
  __warm_up = true;
  __scrolling_complete = false;
  __continue_scroll_test();
}


function __compute_results() {
  __mean = __delta_sum / __n_samples;
  __mean_fps = 1000.0 / __mean;
  __variance = __delta_square_sum / __n_samples - __mean * __mean;
  __sigma = Math.sqrt(__variance);
}


function __print_results() {
  window.console.log(__n_samples + " scroll requests of " + __scroll_delta +
                     " pixels each");
  window.console.log("time between frames (ms):");
  window.console.log("mean = " + __mean);
  window.console.log("sigma = " + __sigma)
  window.console.log("min = " + __delta_min);
  window.console.log("max = " + __delta_max);
  window.console.log("FPS = " + __mean_fps);
}

__scroll_test();
