// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

function testWindowGetsFocus(win) {
  // If the window is already focused, we are done.
  if (win.contentWindow.document.hasFocus()) {
    chrome.test.assertTrue(win.contentWindow.document.hasFocus(),
                           "window has been focused");
    win.close();
    return;
  }

  // Otherwise, we wait for the focus event.
  win.contentWindow.onfocus = callbackPass(function() {
    chrome.test.assertTrue(win.contentWindow.document.hasFocus(),
                           "window has been focused");
    win.close();
  });
}

function testWindowNeverGetsFocus(win) {
  win.contentWindow.onfocus = function() {
    chrome.test.assertFalse(win.contentWindow.document.hasFocus(),
                            "window should not be focused");
    win.close();
  };

  if (win.contentWindow.document.hasFocus()) {
    chrome.test.assertFalse(win.contentWindow.document.hasFocus(),
                            "window should not be focused");
    win.close();
    return;
  };

  if (win.contentWindow.document.readyState == 'complete') {
    chrome.test.assertFalse(win.contentWindow.document.hasFocus(),
                            "window should not be focused");
    win.close();
    return;
  }

  win.contentWindow.onload = callbackPass(function() {
    chrome.test.assertFalse(win.contentWindow.document.hasFocus(),
                            "window should not be focused");
    win.close();
  });
}

function testCreate() {
  chrome.test.runTests([
    function createUnfocusedWindow() {
      chrome.app.window.create('test.html', {
        innerBounds: { width: 200, height: 200 },
        focused: false
      }, callbackPass(testWindowNeverGetsFocus));
    },
    function createTwiceUnfocused() {
      chrome.app.window.create('test.html', {
        id: 'createTwiceUnfocused', focused: false,
        innerBounds: { width: 200, height: 200 }
      }, callbackPass(function(win) {
        win.contentWindow.onload = callbackPass(function() {
          chrome.app.window.create('test.html', {
            id: 'createTwiceUnfocused', focused: false,
            innerBounds: { width: 200, height: 200 }
          }, callbackPass(testWindowNeverGetsFocus));
        });
      }));
    },
    function createFocusedWindow() {
      chrome.app.window.create('test.html', {
        innerBounds: { width: 200, height: 200 },
        focused: true
      }, callbackPass(testWindowGetsFocus));
    },
    function createDefaultFocusStateWindow() {
      chrome.app.window.create('test.html', {
        innerBounds: { width: 200, height: 200 },
      }, callbackPass(testWindowGetsFocus));
    },
    function createTwiceFocusUnfocus() {
      chrome.app.window.create('test.html', {
        id: 'createTwiceFocusUnfocus', focused: true,
        innerBounds: { width: 200, height: 200 }
      }, callbackPass(function(win) {
        win.contentWindow.onload = callbackPass(function() {
          chrome.app.window.create('test.html', {
            id: 'createTwiceFocusUnfocus', focused: false,
            innerBounds: { width: 200, height: 200 }
          }, callbackPass(testWindowGetsFocus));
        });
      }));
    },
    function createTwiceUnfocusFocus() {
      chrome.app.window.create('test.html', {
        id: 'createTwiceUnfocusFocus', focused: false,
        innerBounds: { width: 200, height: 200 }
      }, callbackPass(function(win) {
        win.contentWindow.onload = callbackPass(function() {
          chrome.app.window.create('test.html', {
            id: 'createTwiceUnfocusFocus', focused: true,
            innerBounds: { width: 200, height: 200 }
          }, callbackPass(function() {
            // This test fails on Linux GTK, see http://crbug.com/325219
            // And none of those tests run on Linux Aura, see
            // http://crbug.com/325142
            // We remove this and disable the entire test for Linux GTK when the
            // test will run on other platforms, see http://crbug.com/328829
            if (navigator.platform.indexOf('Linux') != 0)
              testWindowGetsFocus(win);
          }));
        });
      }));
    },
  ]);
}

function testShow() {
  chrome.test.runTests([
    function createUnfocusThenShow() {
      chrome.app.window.create('test.html', {
        id: 'createUnfocusThenShow', focused: false,
        innerBounds: { width: 200, height: 200 }
      }, callbackPass(function(win) {
        win.contentWindow.onload = callbackPass(function() {
          win.show();
          // This test fails on Linux GTK, see http://crbug.com/325219
          // And none of those tests run on Linux Aura, see
          // http://crbug.com/325142
          // We remove this and disable the entire test for Linux GTK when the
          // test will run on other platforms, see http://crbug.com/328829
          if (navigator.platform.indexOf('Linux') != 0)
            testWindowGetsFocus(win);
        });
      }));
    },
    function createUnfocusThenShowUnfocused() {
      chrome.app.window.create('test.html', {
        id: 'createUnfocusThenShowUnfocused', focused: false,
        innerBounds: { width: 200, height: 200 }
      }, callbackPass(function(win) {
        win.contentWindow.onload = callbackPass(function() {
          win.show(false);
          testWindowNeverGetsFocus(win);
        });
      }));
    },
    function createUnfocusThenShowFocusedThenShowUnfocused() {
      chrome.app.window.create('test.html', {
        id: 'createUnfocusThenShowFocusedThenShowUnfocused', focused: false,
        innerBounds: { width: 200, height: 200 }
      }, callbackPass(function(win) {
        win.contentWindow.onload = callbackPass(function() {
          win.show(true);
          win.show(false);
          // This test fails on Linux GTK, see http://crbug.com/325219
          // And none of those tests run on Linux Aura, see
          // http://crbug.com/325142
          // We remove this and disable the entire test for Linux GTK when the
          // test will run on other platforms, see http://crbug.com/328829
          if (navigator.platform.indexOf('Linux') != 0)
            testWindowGetsFocus(win);
        });
      }));
    },
  ]);
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.sendMessage('Launched', function(reply) {
    window[reply]();
  });
});
