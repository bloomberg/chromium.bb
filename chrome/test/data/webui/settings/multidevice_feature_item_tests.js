// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Multidevice', function() {
  let featureItem = null;
  /** @type {!settings.Route} */
  let initialRoute;

  const FAKE_NAME = 'TelePhone';
  const FAKE_SUMMARY_HTML =
      'Teleports your phone to your ChromeBook. <a href="link">Learn more.</a>';
  const FAKE_ICON_NAME = 'settings:tele-phone';

  /**
   * Verifies that the current route is not initialRoute and then navigates
   * back to initialRoute.
   */
  function returnToInitialRoute() {
    Polymer.dom.flush();
    assertNotEquals(initialRoute, settings.getCurrentRoute());
    settings.navigateTo(initialRoute);
    assertEquals(initialRoute, settings.getCurrentRoute());
  }

  setup(function() {
    PolymerTest.clearBody();
    featureItem = document.createElement('settings-multidevice-feature-item');
    assertTrue(!!featureItem);

    initialRoute = settings.routes.MULTIDEVICE_FEATURES;
    settings.routes.TELE_PHONE =
        settings.routes.BASIC.createSection('/telePhone');

    featureItem.featureName = FAKE_NAME;
    featureItem.featureSummaryHtml = FAKE_SUMMARY_HTML;
    featureItem.iconName = FAKE_ICON_NAME;
    featureItem.subpageRoute = settings.routes.TELE_PHONE;

    settings.navigateTo(initialRoute);

    document.body.appendChild(featureItem);
    Polymer.dom.flush();
  });

  teardown(function() {
    featureItem.remove();
  });

  test('click navigates only if it is not on a link', function() {
    featureItem.$$('#item-text-container').click();
    returnToInitialRoute();

    featureItem.$$('iron-icon').click();
    returnToInitialRoute();

    const summaryDiv = featureItem.$$('#featureSecondary');

    summaryDiv.click();
    returnToInitialRoute();

    const link = summaryDiv.querySelector('a');

    assertTrue(!!link);
    link.click();

    Polymer.dom.flush();
    assertEquals(initialRoute, settings.getCurrentRoute());
  });
});
