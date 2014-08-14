// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var idString = "id1";

function testOnCreated() {

  var content = {
    type: "basic",
    iconUrl: "icon.png",
    title: "Title",
    message: "This is the message."
  };

  var createCallback = function (id) {}
  chrome.notifications.create(idString, content, createCallback);

  chrome.notificationProvider.onCreated.addListener(function(senderId,
                                                             notificationId,
                                                             options) {
    var str = notificationId.split("-");
    chrome.test.assertEq(idString, str[1]);
    chrome.test.assertEq(options.title, content.title);
    chrome.test.assertEq(options.message, content.message);
    chrome.test.succeed();
  });
};

chrome.test.runTests([ testOnCreated ]);