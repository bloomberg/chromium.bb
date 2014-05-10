// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  /**
   * Test that some key elements exist and that they have the appropriate
   * constructor name. This verifies that polymer is working correctly.
   */
  function testHasElements() {
    var elementNames = [
      'viewer-toolbar',
      'viewer-page-indicator',
      'viewer-progress-bar',
      'viewer-password-screen',
      'viewer-error-screen'
    ];
    for (var i = 0; i < elementNames.length; i++) {
      var elements = document.querySelectorAll(elementNames[i]);
      chrome.test.assertEq(1, elements.length);
      var element = elements[0];
      chrome.test.assertTrue(
          String(element.constructor).indexOf(elementNames[i]) != -1);
    }
    chrome.test.succeed();
  },

  /**
   * Test that the plugin element exists and is navigated to the correct URL.
   */
  function testPluginElement() {
    var plugin = document.getElementById('plugin');
    chrome.test.assertEq('object', plugin.localName);

    chrome.test.assertTrue(
        plugin.getAttribute('src').indexOf('/pdf/test.pdf') != -1);
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
