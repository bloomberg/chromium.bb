// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  chrome.extension.onRequest.addListener(
    function(request, sender, sendResponse) {
      if (request == 'parent')
        chrome.test.succeed();
      else
        chrome.test.fail('Unexpected request: ' + JSON.stringify(request));
    }
  );
  chrome.test.runTests([
    function testDontInjectInAboutBlankFrame() {
      chrome.test.log("Creating tab...");
      var test_url =
          ("http://localhost:PORT/extensions/" +
           "test_file_with_about_blank_iframe.html")
              .replace(/PORT/, config.testServer.port);
      chrome.tabs.create({ url: test_url });
    },
    function testDontInjectInAboutSrcdocFrame() {
      chrome.test.log('Creating tab...');
      var test_url =
          ('http://localhost:PORT/extensions/' +
           'api_test/webnavigation/srcdoc/a.html')
              .replace(/PORT/, config.testServer.port);
      chrome.tabs.create({ url: test_url });
    },
    function testDontInjectInNestedAboutFrames() {
      chrome.test.log('Creating tab...');
      var test_url =
          ('http://localhost:PORT/extensions/' +
           'test_file_with_about_blank_in_srcdoc.html')
              .replace(/PORT/, config.testServer.port);
      chrome.tabs.create({ url: test_url });
    }
  ]);
});
