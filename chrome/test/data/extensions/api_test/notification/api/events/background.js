// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const notification = chrome.experimental.notification;

const replaceIdString = "foo123";

var testBasicEvents = function() {
  var incidents = 0;

  var onShowCallback = function(info) {
    incidents++;
  }

  var onDisplayed = function(replaceId) {
    incidents++;
    if (incidents == 2) {
      chrome.test.assertEq(replaceIdString, replaceId);
      chrome.test.succeed();
    }
  }
  notification.onDisplayed.addListener(onDisplayed);

  var options = {
    type: "base",
    iconUrl: "http://www.google.com/intl/en/chrome/assets/" +
        "common/images/chrome_logo_2x.png",
    title: "Attention!",
    message: "Check out Cirque du Soleil",
    replaceId: replaceIdString
  };
  notification.show(options, onShowCallback);
};

chrome.test.runTests([ testBasicEvents ]);
