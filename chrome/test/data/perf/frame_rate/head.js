// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var __running = false;
var __old_title = "";
var __raf;

var __t_last;
var __t_est;
var __t_est_total;
var __t_est_squared_total;
var __t_count;
var __t_start;

var __recording = [];
var __advance_gesture;
var __gestures = {
  steady: [
    {"time_ms":1, "y":0},
    {"time_ms":2, "y":10}
  ],
  reading: [
    {"time_ms":1, "y":0},
    {"time_ms":842, "y":40},
    {"time_ms":858, "y":67},
    {"time_ms":874, "y":94},
    {"time_ms":890, "y":149},
    {"time_ms":907, "y":203},
    {"time_ms":923, "y":257},
    {"time_ms":939, "y":311},
    {"time_ms":955, "y":393},
    {"time_ms":971, "y":542},
    {"time_ms":987, "y":718},
    {"time_ms":1003, "y":949},
    {"time_ms":1033, "y":1071},
    {"time_ms":1055, "y":1288},
    {"time_ms":1074, "y":1790},
    {"time_ms":1090, "y":1898},
    {"time_ms":1106, "y":2007},
    {"time_ms":1122, "y":2129},
    {"time_ms":1138, "y":2278},
    {"time_ms":1154, "y":2346},
    {"time_ms":1170, "y":2373}
  ],
  mouse_wheel: [
    {"time_ms":1, "y":0},
    {"time_ms":164, "y":53},
    {"time_ms":228, "y":106},
    {"time_ms":260, "y":160},
    {"time_ms":292, "y":213},
    {"time_ms":308, "y":266},
    {"time_ms":325, "y":320},
    {"time_ms":341, "y":373},
    {"time_ms":357, "y":426},
    {"time_ms":373, "y":480},
    {"time_ms":389, "y":533},
    {"time_ms":405, "y":640},
    {"time_ms":421, "y":693}
  ],
  mac_fling: [
    {"time_ms":1, "y":0},
    {"time_ms":212, "y":1},
    {"time_ms":228, "y":7},
    {"time_ms":261, "y":59},
    {"time_ms":277, "y":81},
    {"time_ms":293, "y":225},
    {"time_ms":309, "y":305},
    {"time_ms":325, "y":377},
    {"time_ms":342, "y":441},
    {"time_ms":358, "y":502},
    {"time_ms":374, "y":559},
    {"time_ms":391, "y":640},
    {"time_ms":408, "y":691},
    {"time_ms":424, "y":739},
    {"time_ms":441, "y":784},
    {"time_ms":458, "y":827},
    {"time_ms":523, "y":926},
    {"time_ms":540, "y":955},
    {"time_ms":560, "y":1007},
    {"time_ms":577, "y":1018},
    {"time_ms":593, "y":1040},
    {"time_ms":611, "y":1061},
    {"time_ms":627, "y":1080},
    {"time_ms":643, "y":1099},
    {"time_ms":659, "y":1115},
    {"time_ms":677, "y":1146},
    {"time_ms":693, "y":1160},
    {"time_ms":727, "y":1173},
    {"time_ms":743, "y":1190},
    {"time_ms":759, "y":1201},
    {"time_ms":776, "y":1210},
    {"time_ms":792, "y":1219},
    {"time_ms":809, "y":1228},
    {"time_ms":826, "y":1236},
    {"time_ms":843, "y":1243},
    {"time_ms":859, "y":1256},
    {"time_ms":876, "y":1262},
    {"time_ms":893, "y":1268},
    {"time_ms":911, "y":1273},
    {"time_ms":941, "y":1275},
    {"time_ms":958, "y":1282},
    {"time_ms":976, "y":1288},
    {"time_ms":993, "y":1291},
    {"time_ms":1022, "y":1294},
    {"time_ms":1055, "y":1302}
  ],
};

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

function __advance_gesture_recording() {
  var y = document.body.scrollTop;
  // Only add a gesture if the scroll position changes.
  if (__recording.length == 0 || y != __recording[__recording.length - 1].y) {
    var time_ms = new Date().getTime() - __t_start;
    __recording.push({ time_ms: time_ms, y: y });
  }
}

function __create_gesture_function(gestures) {
  var i = 0;
  return function() {
    if (i >= gestures.length) {
      __stop();
      return;
    }
    var time_ms = new Date().getTime() - __t_start;
    if (time_ms < gestures[i].time_ms)
      return;

    // Skip all gestures that occured within the same time interval.
    for (i; i < gestures.length && time_ms >= gestures[i].time_ms; ++i);
    window.scrollBy(0, gestures[i - 1].y - document.body.scrollTop);
  }
}

function __create_repeating_gesture_function(gestures) {
  var dtime_ms = gestures[gestures.length - 1].time_ms;
  var dy = gestures[gestures.length - 1].y;

  // Repeat gestures until a scroll goes out of bounds.
  // Note: modifies a copy of the given gestures array.
  gestures = gestures.slice(0);
  for (var i = 0, y = 0; y >= 0 && y < document.body.scrollHeight; ++i) {
    y = gestures[i].y + dy;
    gestures.push({
      time_ms: gestures[i].time_ms + dtime_ms,
      y: y,
    });
  }
  return __create_gesture_function(gestures);
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
    __advance_gesture();
    __sched_update();
  });
}

function __start_recording() {
  __start(__advance_gesture_recording);
}

function __start(gesture_function) {
  if (__running)
    return;
  // Attempt to create a gesture function from a string name.
  if (typeof gesture_function == "string") {
    if (!__gestures[gesture_function])
      throw new Error("Unrecognized gesture name");
    else
      gesture_function = __create_repeating_gesture_function(
                            __gestures[gesture_function]);
  }
  else if (typeof gesture_function != "function")
    throw new Error("Argument is not a function or gesture name");

  __old_title = document.title;
  __advance_gesture = gesture_function;
  __running = true;
  __t_start = new Date().getTime();
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
