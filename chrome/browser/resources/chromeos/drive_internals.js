// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Updates the Authentication Status section.
 * @param {Object} auth_status Dictionary containing auth status.
 */
function UpdateAuthStatus(auth_status) {
  $('has-refresh-token').textContent = auth_status['has-refresh-token'];
  $('has-access-token').textContent = auth_status['has-access-token'];
}

document.addEventListener('DOMContentLoaded', function() {
  chrome.send('pageLoaded');
});
