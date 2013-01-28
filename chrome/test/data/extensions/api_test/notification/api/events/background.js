// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const notification = chrome.experimental.notification;

var idString = "foo";

var testBasicEvents = function() {
  var incidents = 0;

  var onCreateCallback = function(id) {
    chrome.test.assertTrue(id.length > 0);
    chrome.test.assertEq(idString, id);
    incidents++;
  }

  var onDisplayed = function(id) {
    incidents++;
    if (incidents == 2) {
      chrome.test.assertEq(idString, id);
      chrome.test.succeed();
    }
  }
  notification.onDisplayed.addListener(onDisplayed);

  var options = {
    type: "base",
    iconUrl: "http://www.google.com/intl/en/chrome/assets/" +
        "common/images/chrome_logo_2x.png",
    title: "Attention!",
    message: "Check out Cirque du Soleil"
  };
  notification.create(idString, options, onCreateCallback);
};

chrome.test.runTests([ testBasicEvents ]);
