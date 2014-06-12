// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var kOldInputMethod = "_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us::eng";
var kNewInputMethod = "_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:fr::fra";
var kInvalidInputMethod = "xx::xxx";

// The tests needs to be executed in order.

function setTest() {
  console.log('setTest: Changing input method to: ' + kNewInputMethod);
  chrome.inputMethodPrivate.setCurrentInputMethod(kNewInputMethod,
    function() {
      chrome.test.assertTrue(!chrome.runtime.lastError);
      chrome.test.succeed();
    });
}

function getTest() {
  console.log('getTest: Getting current input method.');
  chrome.inputMethodPrivate.getCurrentInputMethod(function(inputMethod) {
    chrome.test.assertEq(kNewInputMethod, inputMethod);
    chrome.test.succeed();
  });
}

function observeTest() {
  console.log('observeTest: Adding input method event listener.');
  chrome.inputMethodPrivate.onChanged.addListener(function(subfix) {
    chrome.test.assertEq('us::eng', subfix);
    chrome.test.succeed();
  });
  console.log('observeTest: Changing input method to: ' + kOldInputMethod);
  chrome.inputMethodPrivate.setCurrentInputMethod(kOldInputMethod);
}


function setInvalidTest() {
  console.log(
      'setInvalidTest: Changing input method to: ' + kInvalidInputMethod);
  chrome.inputMethodPrivate.setCurrentInputMethod(kInvalidInputMethod,
    function() {
      chrome.test.assertTrue(!!chrome.runtime.lastError);
      chrome.test.succeed();
    });
}

function getListTest() {
  console.log('getListTest: Getting input method list.');
  chrome.inputMethodPrivate.getInputMethods(function(inputMethods) {
    chrome.test.assertEq(6, inputMethods.length);
    var foundOldInputMethod = false;
    var foundNewInputMethod = false;
    for (var i = 0; i < inputMethods.length; ++i) {
      if (inputMethods[i].id == kOldInputMethod)
        foundOldInputMethod = true;
      if (inputMethods[i].id == kNewInputMethod)
        foundNewInputMethod = true;
    }
    chrome.test.assertTrue(foundOldInputMethod);
    chrome.test.assertTrue(foundNewInputMethod);
    chrome.test.succeed();
  });
}

chrome.test.sendMessage('ready');
chrome.test.runTests(
    [setTest, getTest, observeTest, setInvalidTest, getListTest]);
