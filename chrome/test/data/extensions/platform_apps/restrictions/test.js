// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var fail = chrome.test.fail;
var succeed = chrome.test.succeed;

var DEFAULT_EXPECTED_ERROR = "Not available for platform apps.";

function assertThrowsError(method, opt_expectedError) {
  try {
    method();
    fail("error not thrown");
  } catch (e) {
    assertEq(e.message || e, opt_expectedError || DEFAULT_EXPECTED_ERROR);
  }
}

chrome.test.runTests([
  function testDocumentOpen() {
    assertThrowsError(document.open);
    succeed();
  },

  function testDocumentClose() {
    assertThrowsError(document.close);
    succeed();
  },

  function testDocumentWrite() {
    assertThrowsError(document.write);
    succeed();
  },

  function testWindowHistoryBack() {
    assertThrowsError(window.history.back);
    assertThrowsError(history.back);
    succeed();
  },

  function testWindowHistoryForward() {
    assertThrowsError(window.history.forward);
    assertThrowsError(history.forward);
    succeed();
  },

  function testWindowHistoryPushState() {
    assertThrowsError(window.history.pushState);
    assertThrowsError(history.pushState);
    succeed();
  },

  function testWindowHistoryReplaceState() {
    assertThrowsError(window.history.replaceState);
    assertThrowsError(history.replaceState);
    succeed();
  },

  function testWindowHistoryLength() {
    assertThrowsError(function() {
      var length = window.history.length;
      length = history.length;
    });
    succeed();
  },

  function testWindowHistoryState() {
    assertThrowsError(function() {
      var state = window.history.state;
      state = history.state;
    });
    succeed();
  },

  function testWindowFind() {
    assertThrowsError(Window.prototype.find);
    assertThrowsError(window.find);
    assertThrowsError(find);
    succeed();
  },

  function testWindowAlert() {
    assertThrowsError(Window.prototype.alert);
    assertThrowsError(window.alert);
    assertThrowsError(alert);
    succeed();
  },

  function testWindowConfirm() {
    assertThrowsError(Window.prototype.confirm);
    assertThrowsError(window.confirm);
    assertThrowsError(confirm);
    succeed();
  },

  function testWindowPrompt() {
    assertThrowsError(Window.prototype.prompt);
    assertThrowsError(window.prompt);
    assertThrowsError(prompt);
    succeed();
  },

  function testBars() {
    var bars = ['locationbar', 'menubar', 'personalbar',
                'scrollbars', 'statusbar', 'toolbar'];
    for (var x = 0; x < bars.length; x++) {
      assertThrowsError(function() {
        var visible = this[bars[x]].visible;
        visible = window[bars[x]].visible;
      });
    }
    succeed();
  },

  function testBlockedEvents() {
    var eventHandler = function() { fail('event handled'); };
    var blockedEvents = ['unload', 'beforeunload'];

    for (var i = 0; i < blockedEvents.length; ++i) {
      assertThrowsError(function() {
        window['on' + blockedEvents[i]] = eventHandler;
      });
      assertThrowsError(function() {
        window.addEventListener(blockedEvents[i], eventHandler);
      });
      assertThrowsError(function() {
        Window.prototype.addEventListener.apply(window,
            [blockedEvents[i], eventHandler]);
      });
    }

    succeed();
  },

  function testSyncXhr() {
    var xhr = new XMLHttpRequest();
    assertThrowsError(function() {
      xhr.open('GET', 'data:should not load', false);
    }, 'INVALID_ACCESS_ERR: DOM Exception 15');
    succeed();
  }
]);
