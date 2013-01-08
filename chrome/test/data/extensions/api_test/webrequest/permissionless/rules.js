// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See ../test_declarative_permissions.js for the tests that use these rules.

var onRequest = chrome.declarativeWebRequest.onRequest;
var RequestMatcher = chrome.declarativeWebRequest.RequestMatcher;

chrome.test.getConfig(function(config) {
  addRules(config.testServer.port);
});

function addRules(testServerPort) {
  onRequest.addRules(
    [{conditions: [new RequestMatcher({
      url: {hostSuffix: '.a.com',
            schemes: ['https']}})],
      actions: [new chrome.declarativeWebRequest.RedirectRequest({
        redirectUrl: 'http://www.a.com:' + testServerPort +
                     '/files/nonexistent/redirected' })]
     },
     {conditions: [new RequestMatcher({
      url: {hostSuffix: '.a.com',
            pathSuffix: '/b.html'}})],
      actions: [new chrome.declarativeWebRequest.RedirectRequest({
        redirectUrl: 'http://www.c.com:' + testServerPort +
                     '/files/nonexistent/redirected' })]
     },
     {conditions: [new RequestMatcher({
      url: {hostSuffix: '.a.com',
            pathSuffix: '/fake.html'}})],
      actions: [new chrome.declarativeWebRequest.RedirectByRegEx({
        from: '(.*)fake(.*)', to: '$1b$2'
      })]
     },

     {conditions: [new RequestMatcher({url: {pathContains: 'blank'}})],
      actions: [new chrome.declarativeWebRequest.RedirectToEmptyDocument()]
     },
     {conditions: [new RequestMatcher({url: {pathContains: 'cancel'}})],
      actions: [new chrome.declarativeWebRequest.CancelRequest()]
     }],
    function(rules) {
      if (chrome.runtime.lastError)
        chrome.test.fail(chrome.runtime.lastError);
      chrome.test.sendMessage("rules all registered");
    }
  );
}
