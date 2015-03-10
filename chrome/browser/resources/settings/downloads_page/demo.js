// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Wire up the prefs to the downloads page.
window.addEventListener('polymer-ready', function() {
  var page = document.querySelector('cr-settings-downloads-page');
  page.prefs = document.querySelector('cr-settings-prefs');
});
