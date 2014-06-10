// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testSimpleAction() {
    var okButton = rootNode.firstChild().firstChild();
    okButton.addEventListener(EventType.focus, function() {
      chrome.test.succeed();
    }, true);
    okButton.focus();
  }
];

setUpAndRunTests(allTests);
