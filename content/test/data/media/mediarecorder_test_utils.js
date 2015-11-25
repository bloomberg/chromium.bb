// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function getTimeStampDeltas(timeStamps) {
  var old_time_stamp = 0;
  var delta_time = [];
  timeStamps.forEach(function(timeStamp) {
    delta_time.push(timeStamp - old_time_stamp);
    old_time_stamp = timeStamp;
  });
  delta_time.shift();
  return delta_time;
}

// This function will be used only when we need to wait for data gathering.
function waitDuration(duration) {
  return new Promise(function(resolve, reject) {
    console.log('Waiting for ', duration.toString(), 'msec');
    setTimeout(
        function() {
          console.log('Done waiting');
          resolve();
        }, duration);
  });
}

function waitFor(description, predicate) {
  return new Promise(function(resolve, reject) {
    var startTime = new Date();
    console.log('Waiting for', description.toString());
    var check = setInterval(function() {
      var elapsed = new Date() - startTime;
      if (elapsed > 3000) {
        startTime = new Date();
        console.log('Still waiting for satisfaction of ' +
            predicate.toString());
      } else if (predicate()) {
        clearInterval(check);
        resolve();
      }
    }, 50);
  });
}
