// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var __running = false;
var __old_title = "";
var __scroll_by = 300;
var __raf;

var __t_last;
var __t_est;
var __t_est_total;
var __t_est_squared_total;
var __t_count;

function __init_stats() {
  __t_last = undefined;
  __t_est = undefined;
  __t_est_total = 0;
  __t_est_squared_total = 0;
  __t_count = 0;
}
__init_stats();

function __calc_results() {
  var M = __t_est_total / __t_count;
  var X = __t_est_squared_total / __t_count;
  var V = X - M * M;
  var S = Math.sqrt(V);

  var R = new Object();
  R.mean = 1000.0 / M;
  R.sigma = R.mean - 1000.0 / (M + S);
  return R;
}

function __scroll_down() {
  var y = window.scrollY;
  window.scrollBy(0, __scroll_by);
  if (window.scrollY == y)
    __stop();
}

function __update_fps() {
  var t_now = new Date().getTime();
  if (window.__t_last) {
    var t_delta = t_now - __t_last;
    if (window.__t_est) {
      __t_est = (0.1 * __t_est) + (0.9 * t_delta);  // low-pass filter
    } else {
      __t_est = t_delta;
    }
    var fps = 1000.0 / __t_est;
    document.title = "FPS: " + (fps | 0);

    __t_est_total += t_delta;
    __t_est_squared_total += t_delta * t_delta;
    __t_count++;
  }
  __t_last = t_now;
}

function __sched_update() {
  if (!__raf) {
    if ("webkitRequestAnimationFrame" in window)
      __raf = webkitRequestAnimationFrame;
    else if ("mozRequestAnimationFrame" in window)
      __raf = mozRequestAnimationFrame;
  }
  __raf(function() {
    if (!__running)
      return;
    __update_fps();
    __scroll_down();
    __sched_update();
  });
}

function __start() {
  if (__running)
    return;
  __old_title = document.title;
  __running = true;
  __sched_update();
}

function __stop() {
  __running = false;
  document.title = __old_title;
}

function __reset() {
  __stop();
  document.body.scrollTop = 0;
  __init_stats();
}

function __force_compositor() {
  document.body.style.webkitTransform = "translateZ(0)";
}
