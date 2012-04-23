// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

chrome.experimental.app.onLaunched.addListener(function() {
  // Only a few chrome.windows.* API methods are tested, it is assumed that the
  // full API is tested elsewhere.
  chrome.test.runTests([
   function testCreateWindow() {
      var getShellWindowCount = function(callback) {
        chrome.windows.getAll(function(windows) {
          callback(windows.filter(
              function(window) { return window.type == 'shell' }).length);
        });
      };

      getShellWindowCount(callbackPass(function(shellWindowCount) {
        chrome.test.assertEq(0, shellWindowCount);
        chrome.windows.create(
            {type: 'shell', width: 128, height: 128},
            callbackPass(function() {
              getShellWindowCount(callbackPass(function(shellWindowCount) {
                chrome.test.assertEq(1, shellWindowCount);
              }));
            }));
        }));
    },

    function testUpdateWindowWidth() {
      chrome.windows.create(
          {type: 'shell', width: 128, height: 128},
          callbackPass(function(window) {
            chrome.windows.update(
                window.id,
                // 256 is a width that will snap to the Aura grid, thus this
                // should be the actual width on Aura (when read back).
                {width: 256, height: 128},
                callbackPass(function() {
                  // The timeout is rather lame, but reading back the width from
                  // the window manager (on Linux) immediately still reports the
                  // old value.
                  setTimeout(callbackPass(function() {
                    chrome.windows.getCurrent(callbackPass(function(window) {
                      chrome.test.assertEq(256, window.width);
                    }));
                  }), 1000);
                }));
          }));
    },


  ]);
});
