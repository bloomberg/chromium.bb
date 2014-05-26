// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../../../../third_party/polymer/platform/platform.js">
<include src="../../../../third_party/polymer/polymer/polymer.js">

// Define the file-systems element.
Polymer('file-systems', {
  ready: function() {
  },

  /**
   * List of provided file system information maps.
   * @type {Array.<Object>}
   */
  model: []
});

/*
 * Updates the mounted file system list.
 * @param {Object} fileSystems Dictionary containing provided file system
 *     information.
 *
 */
function updateFileSystems(fileSystems) {
  var mountedFileSystems = document.querySelector('#mounted-file-systems');
  mountedFileSystems.model = fileSystems;
  Platform.performMicrotaskCheckpoint();
}

document.addEventListener('DOMContentLoaded', function() {
  chrome.send('updateFileSystems');

  // Refresh periodically.
  setInterval(function() {
    chrome.send('updateFileSystems');
  }, 1000);
});
