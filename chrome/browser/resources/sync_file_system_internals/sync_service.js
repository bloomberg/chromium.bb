// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * WebUI to monitor the Sync File System Service.
 */
var syncService = (function() {
'use strict';

/**
 * Request Sync Service Status.
 * Asynchronous, result via getServiceStatusResult.
 */
function getServiceStatus() {
  chrome.send('getServiceStatus');
}

/**
 * Handles callback from getServiceStatus.
 * @param {string} Service status enum as a string.
 */
function getServiceStatusResult(statusString) {
  console.log('getServiceStatusResult', statusString);
  $('service-status').textContent = statusString;
}

/**
 * Get initial sync service values and set listeners to get updated values.
 */
function main() {
  cr.ui.decorate('tabbox', cr.ui.TabBox);
  getServiceStatus();
}

document.addEventListener('DOMContentLoaded', main);

// Must export callbacks as they are called directly from C++ handler code.
return {
  getServiceStatusResult: getServiceStatusResult
};
})();

