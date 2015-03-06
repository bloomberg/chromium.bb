// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Wire up the prefs to the a11y page.
 */
window.onload = function() {
  var page = document.querySelector('cr-settings-a11y-page');
  page.prefs = document.querySelector('cr-settings-prefs');
};
