// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// i18n api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.I18N --lib=browser_tests

var testCallback = chrome.test.testCallback;
var callbackPass = chrome.test.callbackPass;

chrome.test.getConfig(function(config) {

  var TEST_FILE_URL = "http://localhost:PORT/files/extensions/test_file.html"
      .replace(/PORT/, config.testServer.port);

  chrome.test.runTests([
    function getAcceptLanguages() {
      chrome.i18n.getAcceptLanguages(callbackPass(function(results) {
        chrome.test.assertEq(results.length, 2);
        chrome.test.assertEq(results[0], "en-US");
        chrome.test.assertEq(results[1], "en");
      }));
    },
    function getMessage() {
      var message = chrome.i18n.getMessage("simple_message");
      chrome.test.assertEq(message, "Simple message");

      message = chrome.i18n.getMessage("message_with_placeholders",
                                     ["Cira", "John"]);
      chrome.test.assertEq(message, "Cira and John work for Google");

      message = chrome.i18n.getMessage("message_with_one_placeholder", "19");
      chrome.test.assertEq(message, "Number of errors: 19");

      chrome.test.succeed();
    },
    function getMessageFromContentScript() {
      chrome.extension.onRequest.addListener(
        function(request, sender, sendResponse) {
          chrome.test.assertEq(request, "Number of errors: 19");
        }
      );
      chrome.test.log("Creating tab...");
      chrome.tabs.create({
        url: TEST_FILE_URL
      });
      chrome.test.succeed();
    }
  ]);
});
