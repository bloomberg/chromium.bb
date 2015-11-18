// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testGetAvailableSinks = function() {
  var callback = function(sinks) {
    chrome.test.assertEq(1, sinks.length);
    var sink = sinks[0];
    chrome.test.assertEq(1, sink.id);
    chrome.test.assertEq("Disconnected", sink.state);
    chrome.test.assertEq("sink 1", sink.name);
    chrome.test.succeed("GetAvailableSinks succeded");
  };
  chrome.displaySource.getAvailableSinks(callback);
};

var testOnSinksUpdated = function() {
  var callback = function(sinks) {
    chrome.test.assertEq(2, sinks.length);
    var sink = sinks[1];
    chrome.test.assertEq(2, sink.id);
    chrome.test.assertEq("Disconnected", sink.state);
    chrome.test.assertEq("sink 2", sink.name);
    chrome.test.succeed("onSinksUpdated event delivered");
  };
  chrome.displaySource.onSinksUpdated.addListener(callback);
};

var testRequestAuthentication = function() {
  var callback = function(auth_info) {
    chrome.test.assertEq("PBC", auth_info.method);
    chrome.test.succeed("RequestAuthentication succeded");
  };
  chrome.displaySource.requestAuthentication(1, callback);
};

chrome.test.runTests([testGetAvailableSinks,
                      testOnSinksUpdated,
                      testRequestAuthentication]);
