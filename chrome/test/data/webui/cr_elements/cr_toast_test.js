// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-toast', function() {
  let toast;

  setup(function() {
    PolymerTest.clearBody();
    toast = document.createElement('cr-toast');
    document.body.appendChild(toast);
  });

  test('simple show/hide', function() {
    assertFalse(toast.open);

    toast.show();
    assertTrue(toast.open);

    toast.hide();
    assertFalse(toast.open);
  });

  test('auto hide', function() {
    const mockTimer = new MockTimer();
    mockTimer.install();

    const duration = 100;
    toast.duration = duration;

    toast.show();
    assertTrue(toast.open);

    mockTimer.tick(duration);
    assertFalse(toast.open);

    // Check that multiple shows reset the timeout.
    toast.show();
    mockTimer.tick(duration - 1);
    assertTrue(toast.open);

    toast.show();
    mockTimer.tick(1);
    // |duration| has passed, the next line would fail if the timeout wasn't
    // reset.
    assertTrue(toast.open);

    mockTimer.uninstall();
  });
});
