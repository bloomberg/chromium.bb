// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var checkbox;

function save() {
  chrome.storage.sync.set({prefs: {use_notifications: checkbox.checked}});
}

// Initialize the checkbox checked state from the saved preference.
function main() {
  checkbox = document.getElementById('notifications');
  chrome.storage.sync.get(
      {prefs: {use_notifications: false}},
      function (storage) {
        checkbox.checked = storage.prefs.use_notifications;
        checkbox.addEventListener('click', save);
      });
}

main();
