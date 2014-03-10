// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var kNewInputMethod = "fr::fra";

function setAndGetTest() {
  console.log('Changing input method to: ' + kNewInputMethod);
  chrome.test.sendMessage('setInputMethod:' + kNewInputMethod,
    function (response) {
      chrome.test.assertEq('done', response);
      console.log('Getting current input method.');
      chrome.inputMethodPrivate.get(function (inputMethod) {
        chrome.test.assertEq(kNewInputMethod, inputMethod);
        chrome.test.succeed();
      }
    );
  });
}

function setAndObserveTest() {
  console.log('Adding input method event listener.');
  chrome.inputMethodPrivate.onChanged.addListener(
    function(newInputMethod) {
      chrome.test.assertEq(kNewInputMethod, newInputMethod);
      chrome.test.succeed();
    }
  );
  console.log('Changing input method to: ' + kNewInputMethod);
  chrome.test.sendMessage('setInputMethod:' + kNewInputMethod,
    function (response) {
      chrome.test.assertEq('done', response);
    }
  );
}

chrome.test.sendMessage('ready');
chrome.test.runTests([setAndGetTest, setAndObserveTest]);
