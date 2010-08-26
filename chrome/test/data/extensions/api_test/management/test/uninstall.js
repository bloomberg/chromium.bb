// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function uninstall(name) {
  chrome.management.getAll(function(items) {
    assertNoLastError();
    var old_count = items.length;
    var item = getItemNamed(items, name);
    chrome.management.uninstall(item.id, function() {
      assertNoLastError();
      chrome.management.getAll(function(items2) {
        assertNoLastError();
        assertEq(old_count - 1, items2.length);
        for (var i = 0; i < items2.length; i++) {
          assertFalse(items2[i].name == name);
        }
        succeed();
      });
    });
  });
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

