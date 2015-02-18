// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var checkExtension = function(element, expectedValue) {
  chrome.test.assertEq(expectedValue, element.extension);
};

onload = function() {
  chrome.test.runTests([
    function extensionView() {
      // chrome.test.succeed();
      var firstExtensionId = 'firstExtensionId';
      var secondExtensionId = 'secondExtensionId';
      var src = 'data:text/html,<body>One</body>';

      var extensionview = document.querySelector('extensionview');
      extensionview.connect(firstExtensionId, src);
      checkExtension(extensionview, firstExtensionId);

      extensionview.setAttribute('extension', secondExtensionId);
      checkExtension(extensionview, firstExtensionId);
      chrome.test.succeed();
    }
  ]);
};