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
  });

  teardown(function() { resetBanner.remove(); });

  // Tests that the reset profile banner
  //  - opens the reset profile dialog when the reset button is clicked.
  //  - reset happens when clicking on the dialog's reset button.
  //  - the reset profile dialog is closed after reset is done.
  test('ResetBannerReset', function() {
    var dialog = resetBanner.$$('settings-reset-profile-dialog');
    assertFalse(!!dialog);
    MockInteractions.tap(resetBanner.$.reset);
    Polymer.dom.flush();
    assertTrue(resetBanner.showResetProfileDialog_)
    dialog = resetBanner.$$('settings-reset-profile-dialog');
    assertTrue(!!dialog);

    MockInteractions.tap(dialog.$.reset);

    return browserProxy.whenCalled('performResetProfileSettings')
        .then(PolymerTest.flushTasks)
        .then(function() {
          assertFalse(resetBanner.showResetProfileDialog_);
          dialog = resetBanner.$$('settings-reset-profile-dialog');
          assertFalse(!!dialog);
        });
  });

  // Tests that the reset profile banner removes itself from the DOM when
  // the close button is clicked and that |onHideResetProfileBanner| is
  // called.
  test('ResetBannerClose', function() {
    MockInteractions.tap(resetBanner.$.close);
    assertFalse(!!resetBanner.parentNode);
    return browserProxy.whenCalled('onHideResetProfileBanner');
  });
});
