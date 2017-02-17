// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('BannerTests', function() {
  var resetBanner = null;
  var browserProxy = null;

  setup(function() {
    browserProxy = new reset_page.TestResetBrowserProxy();
    settings.ResetBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    resetBanner = document.createElement('settings-reset-profile-banner');
    document.body.appendChild(resetBanner);
    assertTrue(resetBanner.$.dialog.open);
  });

  teardown(function() { resetBanner.remove(); });

  // Tests that the reset profile banner navigates to the Reset profile dialog
  // URL when the "reset all settings" button is clicked.
  test('ResetBannerReset', function() {
    assertNotEquals(settings.Route.RESET_DIALOG, settings.getCurrentRoute());
    MockInteractions.tap(resetBanner.$.reset);
    assertEquals(settings.Route.RESET_DIALOG, settings.getCurrentRoute());
    assertFalse(resetBanner.$.dialog.open);
  });

  // Tests that the reset profile banner closes itself when the OK button is
  // clicked and that |onHideResetProfileBanner| is called.
  test('ResetBannerOk', function() {
    MockInteractions.tap(resetBanner.$.ok);
    return browserProxy.whenCalled('onHideResetProfileBanner').then(function() {
      assertFalse(resetBanner.$.dialog.open);
    });
  });
});
