// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var firstEnter = true;

chrome.test.getConfig(function(config) {
  chrome.test.log('Creating tab...');

  var URL = 'http://localhost:PORT/files/extensions/test_file_with_body.html';
  var TEST_FILE_URL = URL.replace(/PORT/, config.testServer.port);

  chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
    if (changeInfo.status != 'complete')
      return;
    if (!firstEnter)
      return;
    firstEnter = false;

    // We need to test two different paths, because the message bundles used
    // for localization are loaded differently in each case:
    //   (1) localization upon loading extension scripts
    //   (2) localization upon injecting CSS with JavaScript
    chrome.test.runTests([

    // Tests that CSS loaded automatically from the files specified in the
    // manifest has had __MSG_@@extension_id__ replaced with the actual
    // extension id.
    function extensionIDMessageGetsReplacedInContentScriptCSS() {
      chrome.extension.onRequest.addListener(
        function(request, sender, sendResponse) {
          if (request.tag == 'extension_id') {
            if (request.message == 'passed') {
              chrome.test.succeed();
            } else {
              chrome.test.fail(request.message);
            }
          }
        }
      );
      var script_file = {};
      script_file.file = 'test_extension_id.js';
      chrome.tabs.executeScript(tabId, script_file);
    },

    // First injects CSS into the page through chrome.tabs.insertCSS and then
    // checks that it has had __MSG_text_color__ replaced with the correct
    // message value.
    function textDirectionMessageGetsReplacedInInsertCSSCall() {
      chrome.extension.onRequest.addListener(
        function(request, sender, sendResponse) {
          if (request.tag == 'paragraph_style') {
            if (request.message == 'passed') {
              chrome.test.succeed();
            } else {
              chrome.test.fail(request.message);
            }
          }
        }
      );
      var css_file = {};
      css_file.file = 'test.css';
      chrome.tabs.insertCSS(tabId, css_file, function() {
        var script_file = {};
        script_file.file = 'test_paragraph_style.js';
        chrome.tabs.executeScript(tabId,script_file);
      });
    }

    ]);
  });

  chrome.tabs.create({ url: TEST_FILE_URL });
});
