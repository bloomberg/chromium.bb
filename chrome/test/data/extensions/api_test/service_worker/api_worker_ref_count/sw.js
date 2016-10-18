// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.onmessage = function(e) {
  var fail = function() {
    e.ports[0].postMessage('FAILURE');
  };
  if (e.data == 'sendMessageTest') {
    try {
      chrome.test.sendMessage('CHECK_REF_COUNT', function(reply) {
        e.ports[0].postMessage('Worker reply: ' + reply);
      });
    } catch (e) {
      fail();
    }
  } else {
    fail();
  }
};
