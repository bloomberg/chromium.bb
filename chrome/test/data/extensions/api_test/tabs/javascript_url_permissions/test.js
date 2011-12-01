// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var javaScriptURL = "javascript:void(document.body.bgColor='red')";

  var fixPort = function(url) {
    return url.replace(/PORT/, config.testServer.port);
  };
  var urlA = fixPort("http://a.com:PORT/files/extensions/test_file.html");
  var urlB = fixPort("http://b.com:PORT/files/extensions/test_file.html");

  chrome.tabs.create({ url: urlA }, function(tab) {
    var firstTabId = tab.id;

    chrome.tabs.create({ url: urlB }, function(tab) {
      var secondTabId = tab.id;

      chrome.test.runTests([
        function javaScriptURLShouldFail() {
          chrome.tabs.update(firstTabId, {url: javaScriptURL},
              chrome.test.callbackFail('Cannot access contents of url ' +
                   '"' + urlA + '". Extension manifest must request ' +
                   'permission to access this host.'));
        },

        function javaScriptURLShouldSucceed() {
          chrome.tabs.update(secondTabId, {url: javaScriptURL},
              chrome.test.callbackPass());
        }
      ]);
    });
  });
});
