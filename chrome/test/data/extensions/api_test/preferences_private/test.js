// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function() {
  chrome.preferencesPrivate.getSyncCategoriesWithoutPassphrase(
    chrome.test.callbackPass(
      function(categories) {
        chrome.test.assertTrue(categories.indexOf("Bookmarks") >= 0,
            "Bookmarks is missing.");
        chrome.test.assertTrue(categories.indexOf("Preferences") >= 0,
            "Preferences is missing.");
        chrome.test.assertTrue(categories.indexOf("Autofill") == -1,
            "Autofill present even though it's encrypted.");
        chrome.test.assertTrue(categories.indexOf("Typed URLs") == -1,
            "Typed URLs present even though it's not synced.");
      }
    )
  );
}]);
