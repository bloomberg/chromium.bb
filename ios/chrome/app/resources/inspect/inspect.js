// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Notify the application to start collecting console logs.
 */
function startLogging() {
  chrome.send('setLoggingEnabled', [true]);
  $('start-logging').hidden = true;
  $('stop-logging').hidden = false;
}

/**
 * Notify the application to stop collecting console logs.
 */
function stopLogging() {
  chrome.send('setLoggingEnabled', [false]);
  $('logs').innerHTML = '';
  $('start-logging').hidden = false;
  $('stop-logging').hidden = true;
}

document.addEventListener('DOMContentLoaded', function() {
  $('start-logging').onclick = startLogging;
  $('stop-logging').onclick = stopLogging;
});