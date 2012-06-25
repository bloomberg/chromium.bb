// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Inject this script on any page to measure framerate as the page is scrolled
// twice from top to bottom.
//
// USAGE:
//   1. To start the scrolling, invoke __scroll_test().
//   2. Wait for __scrolling_complete to be true
//   3. Read __initial_frame_times and __repeat_frame_times arrays.


// Function that sets a callback called when the next frame is rendered.
var __set_frame_callback;

// Function that scrolls the page.
var __scroll_by;

// Element that can be scrolled. Must provide scrollTop property.
var __scrollable_element;

// Amount of scrolling at each frame.
var __scroll_delta = 100;

// Number of scrolls to perform.
var __num_scrolls = 2;

// Current scroll position.
var __ypos;

// Time of previous scroll callback execution.
var __start_time = 0;

// True when all scrolling has completed.
var __scrolling_complete = false;

// Array of frame times for each scroll in __num_scrolls.
var __frame_times = [[]];

// Set this to true when scrolling in Gmail.
var __is_gmail_test = false;


// Initializes the platform-independent frame callback scheduler function.
function __init_set_frame_callback() {
  __set_frame_callback = window.requestAnimationFrame ||
                         window.mozRequestAnimationFrame ||
                         window.msRequestAnimationFrame ||
                         window.oRequestAnimationFrame ||
                         window.webkitRequestAnimationFrame;
}


// Initializes the most realistic scrolling method.
function __init_scroll_by() {
  if (__is_gmail_test) {
    __scroll_by = function(x) {
      __scrollable_element.scrollByLines(x / __scroll_delta);
    };
  } else if (window.chrome && window.chrome.benchmarking &&
             window.chrome.benchmarking.smoothScrollBy) {
    __scroll_by = window.chrome.benchmarking.smoothScrollBy;
  } else {
    __scroll_by = window.scrollBy;
  }
}


// Scrolls page down and reschedules itself until it hits the bottom.
// Collects stats along the way.
function __do_scroll(now_time) {
  __scroll_by(0, __scroll_delta);
  __set_frame_callback(function(now_time) {
    if (__start_time) {
      if (__scrollable_element.scrollTop > __ypos) {
        // Scroll in progress, push a frame.
        __frame_times[__frame_times.length-1].push(now_time - __start_time);
      } else {
        // Scroll complete, either scroll again or finish.
        if (__frame_times.length < __num_scrolls) {
          __scrollable_element.scrollTop = 0;
          __frame_times.push([]);
        } else {
          console.log('frame_times', '' + __frame_times);
          __scrolling_complete = true;
          return;
        }
      }
    }
    __ypos = __scrollable_element.scrollTop;
    __start_time = now_time;
    __do_scroll();
  });
}


function __start_scroll(scrollable_element) {
  __scrollable_element = scrollable_element;
  __init_scroll_by();
  __set_frame_callback(__do_scroll);
}


// Performs the scroll test.
function __scroll_test() {
  __init_set_frame_callback();
  if (__is_gmail_test) {
    gmonkey.load("2.0", function(api) {
        __start_scroll(api.getScrollableElement());
    });
  } else {
    if (window.performance.timing.loadEventStart) {
      __start_scroll(document.body);  // Page already loaded.
    } else {
      window.addEventListener('load', function() {
        __start_scroll(document.body);  // Page hasn't loaded yet, schedule.
      });
    }
  }
}
