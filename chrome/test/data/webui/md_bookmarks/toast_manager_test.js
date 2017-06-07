// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-toast-manager>', function() {
  var toastManager;
  var store;

  setup(function() {
    toastManager = document.createElement('bookmarks-toast-manager');
    replaceBody(toastManager);
  });

  test('simple show/hide', function() {
    toastManager.show('test', false);
    assertTrue(toastManager.open_);
    assertEquals('test', toastManager.$.content.textContent);
    assertTrue(toastManager.$$('paper-button').hidden);

    toastManager.hide();
    assertFalse(toastManager.open_);

    toastManager.show('test', true);
    assertFalse(toastManager.$$('paper-button').hidden);
  });

  test('auto hide', function() {
    toastManager.duration = 100;

    var timeoutFunc = null;
    var timeoutCounter = 0;
    var clearedTimeout = null;
    toastManager.setTimeout_ = function(f) {
      timeoutFunc = f;
      return timeoutCounter++;
    };
    toastManager.clearTimeout_ = function(n) {
      clearedTimeout = n;
    };

    toastManager.show('test', false);
    assertEquals(0, toastManager.hideTimeout_);
    assertTrue(toastManager.open_);

    timeoutFunc();
    assertEquals(null, toastManager.hideTimeout_);
    assertFalse(toastManager.open_);

    // Check that multiple shows reset the timeout.
    toastManager.show('test', false);
    assertEquals(1, toastManager.hideTimeout_);
    assertTrue(toastManager.open_);

    toastManager.show('test2', false);
    assertEquals(1, clearedTimeout);
    assertEquals(2, toastManager.hideTimeout_);
    assertTrue(toastManager.open_);
  });
});
