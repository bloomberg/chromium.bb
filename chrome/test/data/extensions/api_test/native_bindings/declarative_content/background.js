// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Register a rule to show the page action whenever we see a page with 'example'
// in the host. Send messages after registration of the rule is complete and
// when the page action is clicked.

const kRuleId = 'rule1';

var rule = {
  conditions: [
    new chrome.declarativeContent.PageStateMatcher(
        {pageUrl: {hostPrefix: 'example'}}),
  ], actions: [
    new chrome.declarativeContent.ShowPageAction(),
  ],
  id: kRuleId,
};

chrome.pageAction.onClicked.addListener(function() {
  chrome.declarativeContent.onPageChanged.removeRules([kRuleId], function() {
    chrome.declarativeContent.onPageChanged.getRules(function(rules) {
      chrome.test.assertEq(0, rules.length);
      chrome.test.sendMessage('clicked and removed');
    });
  });
});

chrome.declarativeContent.onPageChanged.addRules([rule], function() {
  chrome.declarativeContent.onPageChanged.getRules(function(rules) {
    chrome.test.assertEq(1, rules.length);
    chrome.test.assertEq(kRuleId, rules[0].id);
    chrome.test.sendMessage('ready');
  });
});
