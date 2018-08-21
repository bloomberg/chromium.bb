// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Multidevice', function() {
  let featureItem = null;
  /** @type {!settings.Route} */
  let initialRoute;

  // Fake MultiDeviceFeature enum value
  const FAKE_MULTIDEVICE_FEATURE = -1;
  const FAKE_SUMMARY_HTML = 'Gives you candy <a href="link">Learn more.</a>';

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

    featureItem.getFeatureSummaryHtml = () => FAKE_SUMMARY_HTML;
    featureItem.feature = FAKE_MULTIDEVICE_FEATURE;

    initialRoute = settings.routes.MULTIDEVICE_FEATURES;
    settings.routes.FREE_CANDY =
        settings.routes.BASIC.createSection('/freeCandy');
    featureItem.subpageRoute = settings.routes.FREE_CANDY;

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
