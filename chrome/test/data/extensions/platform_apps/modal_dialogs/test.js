// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testAlert() {
    alert("You shouldn't see this.... woot!");
    chrome.test.succeed();
  },

  function testConfirm() {
    chrome.test.assertFalse(confirm("Should this test fail?"));
    chrome.test.succeed();
  },

  function testPrompt() {
    chrome.test.assertEq(null, prompt("Return null to pass!"));
    chrome.test.succeed();
  }
]);
