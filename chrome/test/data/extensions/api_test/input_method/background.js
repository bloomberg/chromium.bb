// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var kNewInputMethodTemplate = '_comp_ime_{EXT_ID}xkb:fr::fra';
var kInitialInputMethodRegex = /_comp_ime_([a-z]{32})xkb:us::eng/;
var kInvalidInputMethod = 'xx::xxx';

var testParams = {
  initialInputMethod: '',
  newInputMethod: ''
};

// The tests needs to be executed in order.

function initTests() {
  console.log('initTest: Getting initial inputMethod');
  chrome.inputMethodPrivate.getCurrentInputMethod(function(inputMethod) {
    testParams.initialInputMethod = inputMethod;

    var match = inputMethod.match(kInitialInputMethodRegex);
    chrome.test.assertTrue(!!match);
    chrome.test.assertEq(2, match.length);
    var extensionId = match[1];
    testParams.newInputMethod =
        kNewInputMethodTemplate.replace('{EXT_ID}', extensionId);
    chrome.test.succeed();
  });
}

function setTest() {
  chrome.test.assertTrue(!!testParams.newInputMethod);
  console.log(
      'setTest: Changing input method to: ' + testParams.newInputMethod);
  chrome.inputMethodPrivate.setCurrentInputMethod(testParams.newInputMethod,
    function() {
      chrome.test.assertTrue(
          !chrome.runtime.lastError,
          chrome.runtime.lastError ? chrome.runtime.lastError.message : '');
      chrome.test.succeed();
    });
}

function getTest() {
  chrome.test.assertTrue(!!testParams.newInputMethod);
  console.log('getTest: Getting current input method.');
  chrome.inputMethodPrivate.getCurrentInputMethod(function(inputMethod) {
    chrome.test.assertEq(testParams.newInputMethod, inputMethod);
    chrome.test.succeed();
  });
}

function observeTest() {
  chrome.test.assertTrue(!!testParams.initialInputMethod);
  console.log('observeTest: Adding input method event listener.');

  var listener = function(subfix) {
    chrome.inputMethodPrivate.onChanged.removeListener(listener);
    chrome.test.assertEq('us::eng', subfix);
    chrome.test.succeed();
  };
  chrome.inputMethodPrivate.onChanged.addListener(listener);

  console.log('observeTest: Changing input method to: ' +
                  testParams.initialInputMethod);
  chrome.inputMethodPrivate.setCurrentInputMethod(
      testParams.initialInputMethod);
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
  chrome.test.assertTrue(!!testParams.initialInputMethod);
  chrome.test.assertTrue(!!testParams.newInputMethod);
  console.log('getListTest: Getting input method list.');

  chrome.inputMethodPrivate.getInputMethods(function(inputMethods) {
    chrome.test.assertEq(6, inputMethods.length);
    var foundInitialInputMethod = false;
    var foundNewInputMethod = false;
    for (var i = 0; i < inputMethods.length; ++i) {
      if (inputMethods[i].id == testParams.initialInputMethod)
        foundInitialInputMethod = true;
      if (inputMethods[i].id == testParams.newInputMethod)
        foundNewInputMethod = true;
    }
    chrome.test.assertTrue(foundInitialInputMethod);
    chrome.test.assertTrue(foundNewInputMethod);
    chrome.test.succeed();
  });
}

function initDictionaryNotLoadedTest() {
  // This test must come first because the dictionary is only lazy loaded after
  // this call is made.
  chrome.inputMethodPrivate.fetchAllDictionaryWords(function(words) {
    chrome.test.assertTrue(words === undefined);
    chrome.test.assertTrue(!!chrome.runtime.lastError);
    chrome.test.succeed();
  });
}

function initDictionaryTests() {
  chrome.inputMethodPrivate.fetchAllDictionaryWords(function(words) {
    chrome.test.assertTrue(words !== undefined);
    chrome.test.assertTrue(words.length === 0);
    chrome.test.succeed();
  });
}

function addWordToDictionaryTest() {
  chrome.inputMethodPrivate.addWordToDictionary('helloworld', function() {
    chrome.inputMethodPrivate.fetchAllDictionaryWords(function(words) {
      chrome.test.assertTrue(words.length === 1);
      chrome.test.assertEq(words[0], 'helloworld');
      chrome.test.succeed();
    });
  });
}

chrome.test.sendMessage('ready');
chrome.test.runTests(
    [initTests, setTest, getTest, observeTest, setInvalidTest, getListTest,
     initDictionaryNotLoadedTest, initDictionaryTests,
     addWordToDictionaryTest]);
