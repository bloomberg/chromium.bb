// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var declarative = chrome.declarative;

var PageStateMatcher = chrome.declarativeContent.PageStateMatcher;
var ShowPageAction = chrome.declarativeContent.ShowPageAction;

var rule0 = {
  conditions: [new PageStateMatcher({pageUrl: {hostPrefix: "test1"}}),
               new PageStateMatcher({css: ["input[type='password']"]})],
  actions: [new ShowPageAction()]
}

var testEvent = chrome.declarativeContent.onPageChanged;

testEvent.removeRules(undefined, function() {
  testEvent.addRules([rule0], function() {
    chrome.test.sendMessage("ready", function(reply) {
    })
  });
});
