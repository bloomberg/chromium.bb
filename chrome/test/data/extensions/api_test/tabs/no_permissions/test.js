// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assertNoSensitiveFields(tab) {
  ['url', 'title', 'faviconUrl'].forEach(function(field) {
    chrome.test.assertEq(undefined, tab[field],
                         'Sensitive property ' + field + ' is visible')
  });
}

chrome.test.runTests([
  function testOnUpdated() {
    var neededCallbacks = 3;  // two onUpdateListener calls, one create callback
    var countDown = function() {
      neededCallbacks--;
      if (neededCallbacks == 0) {
        chrome.tabs.onUpdated.removeListener(onUpdateListener);
        chrome.test.succeed();
      }
    };

    var onUpdateListener = function(tabId, info, tab) {
      assertNoSensitiveFields(info);
      assertNoSensitiveFields(tab);
      countDown();
    }

    chrome.tabs.onUpdated.addListener(onUpdateListener);
    chrome.tabs.create({url: 'chrome://newtab/'}, function(tab) {
      assertNoSensitiveFields(tab);
      chrome.tabs.update(tab.id, {url: 'about:blank'}, function(tab) {
        assertNoSensitiveFields(tab);
        countDown();
      });
    });
  },

  function testQuery() {
    chrome.tabs.create({url: 'chrome://newtab/'});
    chrome.tabs.query({active: true}, chrome.test.callbackPass(function(tabs) {
      chrome.test.assertEq(1, tabs.length);
      assertNoSensitiveFields(tabs[0]);
    }));
  },

]);
