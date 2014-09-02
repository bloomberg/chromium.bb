// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Overrides fileBrowserPrivate.getDownloadUrl
 * @param {string} url
 * @param {function(string)} callback
 */
chrome.fileBrowserPrivate.getDownloadUrl = function(url, callback) {
  var dummyUrl = 'http://example.com/test.mp4?access_token=ACCESSTOKEN;
  setTimeout(callback.bind(null, dummyUrl));
};
