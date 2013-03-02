// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var prefs = new Prefs;

// Initialize the checkbox checked state from the saved preference.
function main() {
  var checkbox = document.getElementById('notifications');
  prefs.getUseNotifications(function(useNotifications) {
    checkbox.checked = useNotifications;
    checkbox.addEventListener(
      'click',
      function() {prefs.setUseNotifications(checkbox.checked);});
  });

  var textbox = document.getElementById('try-job-username');
  prefs.getTryJobUsername(function(tryJobUsername) {
    textbox.value = tryJobUsername;
    textbox.addEventListener(
        'change',
        function() {prefs.setTryJobUsername(textbox.value);});
  });
}

main();
