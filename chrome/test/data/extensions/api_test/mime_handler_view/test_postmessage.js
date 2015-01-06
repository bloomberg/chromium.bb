// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  function testPostMessage() {
    var messages = ['hey', 100, 25.0];
    var messages_sent = 0;
    var messages_received = 0;

    window.addEventListener('message', function(event) {
      if (event.data == messages[messages_received])
        messages_received++;
      else
        chrome.test.fail();

      if (messages_received == messages.length)
        chrome.test.succeed();
    }, false);

    var plugin = document.getElementById('plugin');
    function postNextMessage() {
      plugin.postMessage(messages[messages_sent]);
      messages_sent++;
      if (messages_sent < messages.length)
        setTimeout(postNextMessage, 0);
    }
    postNextMessage();
  },
];

chrome.test.runTests(tests);
