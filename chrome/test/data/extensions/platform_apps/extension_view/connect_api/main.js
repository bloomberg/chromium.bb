// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var checkExtension = function(element, expectedValue) {
  chrome.test.assertEq(expectedValue, element.extension);
};

var checkSrc = function(element, expectedValue) {
  chrome.test.assertEq(expectedValue, element.src);
};

onload = function() {
  chrome.test.runTests([
    function extensionView() {
      var firstExtensionId = 'firstExtensionId';
      var secondExtensionId = 'secondExtensionId';
      var firstSrc = 'data:text/html,<body>One</body>';
      var secondSrc = 'data:text/html,<body>One</body>';

      var extensionview = document.querySelector('extensionview');
      // Call connect with an initial extension Id and src.
      extensionview.connect(firstExtensionId, firstSrc);
      checkExtension(extensionview, firstExtensionId);
      checkSrc(extensionview, firstSrc);

      // Call connect with the same extension Id and src.
      extensionview.connect(firstExtensionId, firstSrc);
      checkExtension(extensionview, firstExtensionId);
      checkSrc(extensionview, firstSrc);

      // Call connect with the same extension Id and different src.
      extensionview.connect(firstExtensionId, secondSrc);
      checkExtension(extensionview, firstExtensionId);
      checkSrc(extensionview, secondSrc);

      // Call connect with a new extension Id and src.
      extensionview.connect(secondExtensionId, firstSrc);
      checkExtension(extensionview, secondExtensionId);
      checkSrc(extensionview, firstSrc);

      // Call setAttribute with a different src.
      extensionview.setAttribute('src', secondSrc);
      checkExtension(extensionview, secondExtensionId);
      checkSrc(extensionview, secondSrc);
      chrome.test.succeed();
    }
  ]);
};