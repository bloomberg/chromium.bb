// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function uninstall(name) {
  var expected_id;
  listenOnce(chrome.management.onUninstalled, function(id) {
    assertEq(expected_id, id);
  });

  chrome.management.getAll(callback(function(items) {
    var old_count = items.length;
    var item = getItemNamed(items, name);
    expected_id = item.id;
    chrome.management.uninstall(item.id, callback(function() {
      chrome.management.getAll(callback(function(items2) {
        assertEq(old_count - 1, items2.length);
        for (var i = 0; i < items2.length; i++) {
          assertFalse(items2[i].name == name);
        }
      }));
    }));
  }));
}

var tests = [
  function uninstallEnabledApp() {
    uninstall("enabled_app");
  },

  function uninstallDisabledApp() {
    uninstall("disabled_app");
  },

  function uninstallEnabledExtension() {
    uninstall("enabled_extension");
  },

  function uninstallDisabledExtension() {
    uninstall("disabled_extension");
  }
];

chrome.test.runTests(tests);

