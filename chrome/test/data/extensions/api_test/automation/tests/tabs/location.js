// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testLocation() {
    function assertOkButtonLocation(event) {
      var okButton = rootNode.firstChild().firstChild();
      assertTrue('location' in okButton);
      assertEq({left:100, top: 200, width: 300, height: 400},
               okButton.location);
      chrome.test.succeed();
    };

    var okButton = rootNode.firstChild().firstChild();
    assertTrue('location' in okButton, 'no location in okButton');
    assertTrue('left' in okButton.location, 'no left in location');
    assertTrue('top' in okButton.location, 'no top in location');
    assertTrue('height' in okButton.location, 'no height in location');
    assertTrue('width' in okButton.location, 'no width in location');

    rootNode.addEventListener(
        EventType.childrenChanged, assertOkButtonLocation);
    chrome.tabs.executeScript({ 'code':
          'document.querySelector("button")' +
          '.setAttribute("style", "position: absolute; left: 100; top: 200; ' +
          'width: 300; height: 400;");' });
  }
];

setUpAndRunTests(allTests);
