// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;
var pass = chrome.test.callbackPass;

var NO_TABS_PERMISSION =
    "You do not have permission to use 'windows.getAll'.";

chrome.test.getConfig(function(config) {

  function doReq(domain, callback) {
    var req = new XMLHttpRequest();
    var url = domain + ":PORT/files/extensions/test_file.txt";
    url = url.replace(/PORT/, config.testServer.port);

    chrome.test.log("Requesting url: " + url);
    req.open("GET", url, true);

    req.onload = function() {
      assertEq(200, req.status);
      assertEq("Hello!", req.responseText);
      callback(true);
    };

    req.onerror = function() {
      chrome.test.log("status: " + req.status);
      chrome.test.log("text: " + req.responseText);
      callback(false);
    };

    req.send(null);
  }

  chrome.test.runTests([
    function denyRequest() {
      chrome.permissions.request(
          {permissions: ['tabs'], origins: ['http://*.c.com/*']},
          pass(function(granted) {
            // They were not granted, and there should be no error.
            assertFalse(granted);
            assertTrue(chrome.extension.lastError === undefined);

            // Make sure they weren't granted...
            chrome.permissions.contains(
                {permissions: ['tabs'], origins:['http://*.c.com/*']},
                pass(function(result) { assertFalse(result); }));

            try {
              chrome.windows.getAll({populate: true}, function() {
                chrome.test.fail("Should not have tabs API permission.");
              });
            } catch (e) {
              assertTrue(e.message.indexOf(NO_TABS_PERMISSION) == 0);
            }

            doReq('http://b.c.com/', pass(function(result) {
              assertFalse(result);
            }));
      }));
    }
  ]);
});
