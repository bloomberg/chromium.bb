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
    var timerProxy = new bookmarks.TestTimerProxy();
    timerProxy.immediatelyResolveTimeouts = false;
    toastManager.timerProxy_ = timerProxy;

    toastManager.duration = 100;

    toastManager.show('test', false);
    assertEquals(0, toastManager.hideTimeoutId_);
    assertTrue(toastManager.open_);

    timerProxy.runTimeoutFn(0);
    assertEquals(null, toastManager.hideTimeoutId_);
    assertFalse(toastManager.open_);

    // Check that multiple shows reset the timeout.
    toastManager.show('test', false);
    assertEquals(1, toastManager.hideTimeoutId_);
    assertTrue(toastManager.open_);

    toastManager.show('test2', false);
    assertFalse(timerProxy.hasTimeout(1));
    assertEquals(2, toastManager.hideTimeoutId_);
    assertTrue(toastManager.open_);
  });
});
