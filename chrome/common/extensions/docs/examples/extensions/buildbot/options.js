// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function save() {
  var prefs = JSON.parse(localStorage.prefs);
  prefs.use_notifications = document.getElementById('notifications').checked;
  localStorage.prefs = JSON.stringify(prefs);
}

// Make sure the checkbox checked state gets properly initialized from the
// saved preference.
document.addEventListener('DOMContentLoaded', function () {
  var prefs = JSON.parse(localStorage.prefs);
  document.getElementById('notifications').checked = prefs.use_notifications;
  document.getElementById('notifications').addEventListener('click', save);
});
