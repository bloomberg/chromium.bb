// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('SiteEntry', function() {
  /**
   * An example eTLD+1 Object with multiple origins grouped under it.
   * @type {!SiteGroup}
   */
  const TEST_MULTIPLE_SITE_GROUP = test_util.createSiteGroup('example.com', [
    'http://example.com',
    'https://www.example.com',
    'https://login.example.com',
  ]);

  /**
   * An example eTLD+1 Object with a single origin in it.
   * @type {!SiteGroup}
   */
  const TEST_SINGLE_SITE_GROUP = test_util.createSiteGroup('foo.com', [
    'https://login.foo.com',
  ]);

  const TEST_COOKIE_LIST = {
    id: 'foo',
    children: [
      {},
      {},
      {},
      {},
    ]
  };

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy;

  /**
   * The mock local data proxy object to use during test.
   * @type {TestLocalDataBrowserProxy}
   */
  let localDataBrowserProxy;

  /**
   * A site list element created before each test.
   * @type {SiteList}
   */
  let testElement;

  /**
   * The clickable element that expands to show the list of origins.
   * @type {Element}
   */
  let toggleButton;

  // Initialize a site-list before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    localDataBrowserProxy = new TestLocalDataBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    settings.LocalDataBrowserProxyImpl.instance_ = localDataBrowserProxy;

    PolymerTest.clearBody();
    testElement = document.createElement('site-entry');
    assertTrue(!!testElement);
    document.body.appendChild(testElement);

    toggleButton = testElement.$.toggleButton;
  });

  teardown(function() {
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    settings.resetRouteForTesting();
  });

  test('displays the correct number of origins', function() {
    testElement.siteGroup = TEST_MULTIPLE_SITE_GROUP;
    Polymer.dom.flush();
    assertEquals(
        3, testElement.$.collapseChild.querySelectorAll('.list-item').length);
  });

  test('expands and closes to show more origins', function() {
    testElement.siteGroup = TEST_MULTIPLE_SITE_GROUP;
    assertTrue(testElement.grouped_(testElement.siteGroup));
    assertEquals('false', toggleButton.getAttribute('aria-expanded'));
    const originList = testElement.root.querySelector('iron-collapse');
    assertTrue(originList.classList.contains('iron-collapse-closed'));
    assertEquals('true', originList.getAttribute('aria-hidden'));

    toggleButton.click();
    assertEquals('true', toggleButton.getAttribute('aria-expanded'));
    assertTrue(originList.classList.contains('iron-collapse-opened'));
    assertEquals('false', originList.getAttribute('aria-hidden'));
  });

  test('with single origin navigates to Site Details', function() {
    testElement.siteGroup = TEST_SINGLE_SITE_GROUP;
    assertFalse(testElement.grouped_(testElement.siteGroup));
    assertEquals('false', toggleButton.getAttribute('aria-expanded'));
    const originList = testElement.root.querySelector('iron-collapse');
    assertTrue(originList.classList.contains('iron-collapse-closed'));
    assertEquals('true', originList.getAttribute('aria-hidden'));

    toggleButton.click();
    assertEquals('false', toggleButton.getAttribute('aria-expanded'));
    assertTrue(originList.classList.contains('iron-collapse-closed'));
    assertEquals('true', originList.getAttribute('aria-hidden'));
    assertEquals(
        settings.routes.SITE_SETTINGS_SITE_DETAILS.path,
        settings.getCurrentRoute().path);
    assertEquals(
        'https://login.foo.com', settings.getQueryParameters().get('site'));
  });

  test('with multiple origins navigates to Site Details', function() {
    testElement.siteGroup = TEST_MULTIPLE_SITE_GROUP;
    Polymer.dom.flush();
    const originList =
        testElement.$.collapseChild.querySelectorAll('.list-item');
    assertEquals(3, originList.length);

    // Test clicking on one of these origins takes the user to Site Details,
    // with the correct origin.
    originList[1].click();
    assertEquals(
        settings.routes.SITE_SETTINGS_SITE_DETAILS.path,
        settings.getCurrentRoute().path);
    assertEquals(
        TEST_MULTIPLE_SITE_GROUP.origins[1],
        settings.getQueryParameters().get('site'));
  });

  test('with single origin does not show overflow menu', function() {
    testElement.siteGroup = TEST_SINGLE_SITE_GROUP;
    Polymer.dom.flush();
    const overflowMenuButton = testElement.$.overflowMenuButton;
    assertTrue(overflowMenuButton.closest('.row-aligned').hidden);
  });

  test(
      'with multiple origins can reset settings via overflow menu', function() {
        testElement.siteGroup = TEST_MULTIPLE_SITE_GROUP;
        Polymer.dom.flush();
        const overflowMenuButton = testElement.$.overflowMenuButton;
        assertFalse(overflowMenuButton.closest('.row-aligned').hidden);

        // Open the reset settings dialog and make sure both cancelling the
        // action and resetting all permissions work.
        const overflowMenu = testElement.$.menu.get();
        const menuItems = overflowMenu.querySelectorAll('.dropdown-item');
        ['cancel-button', 'action-button'].forEach(buttonType => {
          // Test clicking on the overflow menu button opens the menu.
          assertFalse(overflowMenu.open);
          overflowMenuButton.click();
          assertTrue(overflowMenu.open);

          // Open the reset settings dialog and tap the |buttonType| button.
          assertFalse(testElement.$.confirmResetSettings.open);
          menuItems[0].click();
          assertTrue(testElement.$.confirmResetSettings.open);
          const actionButtonList =
              testElement.$.confirmResetSettings.getElementsByClassName(
                  buttonType);
          assertEquals(1, actionButtonList.length);
          actionButtonList[0].click();

          // Check the dialog and overflow menu are now both closed.
          assertFalse(testElement.$.confirmResetSettings.open);
          assertFalse(overflowMenu.open);
        });

        // Ensure a call was made to setOriginPermissions for each origin.
        assertEquals(
            TEST_MULTIPLE_SITE_GROUP.origins.length,
            browserProxy.getCallCount('setOriginPermissions'));
      });

  test(
      'moving from grouped to ungrouped does not get stuck in opened state',
      function() {
        // Clone this object to avoid propogating changes made in this test.
        testElement.siteGroup =
            JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
        Polymer.dom.flush();
        toggleButton.click();
        assertTrue(testElement.$.collapseChild.opened);

        // Remove all origins except one, then make sure it's not still
        // expanded.
        testElement.siteGroup.origins.splice(1);
        assertEquals(1, testElement.siteGroup.origins.length);
        testElement.onSiteGroupChanged_(testElement.siteGroup);
        assertFalse(testElement.$.collapseChild.opened);
      });

  test('cookies only show when non-zero for grouped entries', function() {
    localDataBrowserProxy.setCookieDetails(TEST_COOKIE_LIST);
    testElement.siteGroup = TEST_MULTIPLE_SITE_GROUP;
    Polymer.dom.flush();
    const cookiesLabel = testElement.$.cookies;
    assertEquals('', cookiesLabel.textContent.trim());
    assertTrue(cookiesLabel.hidden);

    // When the number of cookies is more than zero, the label appears.
    testElement.onSiteGroupChanged_(TEST_MULTIPLE_SITE_GROUP);
    return localDataBrowserProxy.whenCalled('getNumCookiesString')
        .then((args) => {
          assertEquals('example.com', args);
          assertFalse(cookiesLabel.hidden);
          assertEquals(
              `${TEST_COOKIE_LIST.children.length} cookies`,
              cookiesLabel.textContent.trim());
        });
  });

  test('cookies do not show for ungrouped entries', function() {
    testElement.siteGroup = TEST_SINGLE_SITE_GROUP;
    Polymer.dom.flush();
    const cookiesLabel = testElement.$.cookies;
    assertTrue(cookiesLabel.hidden);
    assertEquals('', cookiesLabel.textContent.trim());

    testElement.onSiteGroupChanged_(TEST_SINGLE_SITE_GROUP);
    // Make sure there was never any call to the back end to retrieve cookies.
    assertEquals(0, localDataBrowserProxy.getCallCount('getNumCookiesString'));
    assertEquals('', cookiesLabel.textContent.trim());
    assertTrue(cookiesLabel.hidden);
  });
});
