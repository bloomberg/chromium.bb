// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// To see chrome.test.log messages execute the test like this:
// $ out/gn/browser_tests --gtest_filter=*PanelTest* --vmodule=test_api=1
chrome.test.log('executing background.js ...');

var oldNumberOfTabs;
chrome.test.runTests([
  function testPanelBrowsingInstance() {
    chrome.test.log('counting all the tabs (before window.open) ...');
    chrome.tabs.query({}, function(tabs) {
      oldNumberOfTabs = tabs.length;
      chrome.test.log('oldNumberOfTabs = ' + oldNumberOfTabs);

      // Note that the background-subframe has to be navigated to foo.com,
      // because otherwise we would get the following error when calling
      // window.open from panel-subframe.js:
      //   Unsafe JavaScript attempt to initiate navigation for frame with URL
      //   'about:blank' from frame with URL
      //   'http://foo.com:41191/extensions/.../panel-subframe.html'.  The frame
      //   attempting navigation is neither same-origin with the target, nor is
      //   it the target's parent or opener.
      chrome.test.getConfig(function(config) {
        var baseUri = 'http://foo.com:' + config.testServer.port +
            '/extensions/api_test/window_open/panel_browsing_instance/';

        chrome.test.log('navigating background-subframe ...');
        var subframe = document.getElementById('background-subframe-id');
        subframe.src = baseUri +
            'background-subframe.html?extensionId=' + chrome.runtime.id;

        // background-subframe.js will send a message back to us -
        // backgroundSubframeNavigated - we will continue execution
        // in the callback below.
      });
    });
  }
]);

chrome.runtime.onMessageExternal.addListener(
  function(request, sender, sendResponse) {
    if (request.backgroundSubframeNavigated) {
      chrome.test.log('got callback from background-subframe.js ...');
      chrome.test.log('creating the panel window ...');
      chrome.windows.create(
          { 'url': chrome.extension.getURL('panel.html'), 'type': 'panel' },
          function(win) {
            chrome.test.assertEq('panel', win.type);
          });

      // panel.js will navigate panel-subframe and then panel-subframe.js will
      // call window.open and send a message back to us (windowOpened) - we will
      // continue execution below.
    } else if (request.windowOpened) {
      chrome.test.log('got callback from panel-subframe.js ...');
      chrome.test.log('counting all the tabs (after window.open) ...');
      chrome.tabs.query({}, function(tabs) {
        var newNumberOfTabs = tabs.length;
        chrome.test.log('newNumberOfTabs = ' + newNumberOfTabs);

        chrome.test.assertEq(newNumberOfTabs, oldNumberOfTabs);
        chrome.test.notifyPass();
      });
    }
  });


