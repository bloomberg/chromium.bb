// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Function that sets a callback called when the next frame is rendered.
var __set_frame_callback;

// Amount of scrolling at each frame.
var __scroll_delta = 500;

// Current scroll position.
var __ypos;

// Time of previous scroll callback execution.
var __start_time = 0;

// Deltas are in milliseconds.
var __delta_max = 0;
var __delta_min = 1000000;
var __delta_sum = 0;
var __delta_square_sum = 0;
var __n_samples = 0;

// Whether all scroll tests have completed.
var __tests_complete = false;

// Initializes the platform-independent frame callback scheduler function.
function __init_set_frame_callback() {
  if (!__set_frame_callback) {
    if ("webkitRequestAnimationFrame" in window)
      __set_frame_callback = webkitRequestAnimationFrame;
    else if ("mozRequestAnimationFrame" in window)
      __set_frame_callback = mozRequestAnimationFrame;
  }
}


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
// stats.
function __frame_callback() {
  record_delta_time();
  window.scrollBy(0, __scroll_delta);
  __ypos += __scroll_delta;
  scrollable_distance = document.body.scrollHeight - window.innerHeight;
  if (__ypos < scrollable_distance) {
    __set_frame_callback(__frame_callback, document.body);
  } else {
    __tests_complete = true;
  }
}

// Sets things here after the document has been rendered.
function __first_frame_callback() {
  document.body.scrollTop = 0;
  __set_frame_callback(__frame_callback, document.body);
}


function __start_scroll_test() {
  __init_set_frame_callback();
  __ypos = 0;
  __set_frame_callback(__first_frame_callback, document.body);
}
