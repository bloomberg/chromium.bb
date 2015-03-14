// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var registration;
var interval;

function waitUntilActivated() {
  if (registration && registration.active) {
    window.location = "#sw_activated";
    window.clearInterval(interval);
  } else {
    console.log("SW not yet active.");
  }
}

function initialize() {
  navigator.serviceWorker.register('service_worker.js')
    .then(function(r) {
            registration = r;
          });
  interval = window.setInterval(function() {waitUntilActivated()}, 50);
}