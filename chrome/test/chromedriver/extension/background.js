// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Gets info about the current window.
 *
 * @param {function(*)} callback The callback to invoke with the window info.
 */
function getWindowInfo(callback) {
  chrome.windows.getCurrent({populate: true}, callback);
}

/**
 * Updates the properties of the current window.
 *
 * @param {Object} updateInfo Update info to pass to chrome.windows.update.
 * @param {function()} callback Invoked when the updating is complete.
 */
function updateWindow(updateInfo, callback) {
  chrome.windows.getCurrent({}, function(window) {
    chrome.windows.update(window.id, updateInfo, callback);
  });
}
