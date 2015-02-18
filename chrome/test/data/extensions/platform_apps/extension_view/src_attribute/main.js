// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var checkSrc = function(element, expectedValue) {
  chrome.test.assertEq(expectedValue, element.src);
};

onload = function() {
  chrome.test.runTests([
    function extensionView() {
      var expectedSrcOne = 'data:text/html,<body>One</body>';
      var expectedSrcTwo = 'data:text/html,<body>Two</body>';

      var extensionview = document.querySelector('extensionview');
      checkSrc(extensionview, expectedSrcOne);

      extensionview.setAttribute('src', expectedSrcTwo);
      checkSrc(extensionview, expectedSrcTwo);
      chrome.test.succeed();
    }
  ]);
};