// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
    function setRestrictedKeyboard() {
      chrome.virtualKeyboard.restrictFeatures({
        autoCompleteEnabled: true,
        autoCorrectEnabled: true,
        spellCheckEnabled: true,
        voiceInputEnabled: true,
        handwritingEnabled: true
      }, chrome.test.callbackPass(function() {
          chrome.test.sendMessage('restrictedChanged',
              function(featuresEnabled) {
                chrome.test.assertEq('true', featuresEnabled);
                chrome.test.succeed();
              });
          }));
    },
    function setNotRestrictedKeyboard() {
      chrome.virtualKeyboard.restrictFeatures({
        autoCompleteEnabled: false,
        autoCorrectEnabled: false,
        spellCheckEnabled: false,
        voiceInputEnabled: false,
        handwritingEnabled: false
      }, chrome.test.callbackPass(function() {
        chrome.test.sendMessage('restrictedChanged',
            function(featuresEnabled) {
              chrome.test.assertEq('false', featuresEnabled);
              chrome.test.succeed();
            });
        }));
    },
    function differentEnabledStates() {
      var kError = 'All features are expected to have the same enabled state.';
      chrome.virtualKeyboard.restrictFeatures({
        autoCompleteEnabled: true,
        autoCorrectEnabled: false,
        spellCheckEnabled: true,
        voiceInputEnabled: false,
        handwritingEnabled: true
      }, chrome.test.callbackFail(kError));
    },
]);
