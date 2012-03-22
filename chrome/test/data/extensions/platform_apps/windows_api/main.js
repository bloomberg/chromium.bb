// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

// Only a few chrome.windows.* API methods are tested, it is assumed that the
// full API is tested elsewhere.
chrome.test.runTests([
  function testGetCurrentWindow() {
    chrome.windows.getCurrent(callbackPass(function(window) {
      chrome.test.assertEq(250, window.width);
      chrome.test.assertEq('shell', window.type);
    }));
  },

  function testUpdateWindowWidth() {
    chrome.windows.getCurrent(callbackPass(function(window) {
      chrome.windows.update(
          window.id, {width: 300}, callbackPass(function() {
            // The timeout is rather lame, but reading back the width from the
            // window manager (on Linux) immediately still reports the old
            // value.
            setTimeout(callbackPass(function() {
              chrome.windows.getCurrent(callbackPass(function(window) {
                chrome.test.assertEq(300, window.width);
              }));
            }), 100);
          }));
    }));
  },

  function testCreateWindow() {
    var getShellWindowCount = function(callback) {
      chrome.windows.getAll(function(windows) {
        callback(windows.filter(
            function(window) { return window.type == 'shell'}).length);
      });
    };

    getShellWindowCount(callbackPass(function(shellWindowCount) {
      chrome.test.assertEq(1, shellWindowCount);
      chrome.windows.create({type: 'shell'}, callbackPass(function() {
        getShellWindowCount(callbackPass(function(shellWindowCount) {
          chrome.test.assertEq(2, shellWindowCount);
        }));
      }));
    }));
  }
]);
