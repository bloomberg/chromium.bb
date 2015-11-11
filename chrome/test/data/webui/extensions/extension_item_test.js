// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-item. */
cr.define('extension_item_tests', function() {
  /**
   * A mock delegate for the item, capable of testing functionality.
   * @constructor
   * @implements {extensions.ItemDelegate}
   */
  function MockDelegate() {}

  MockDelegate.prototype = {
    /**
     * Tests clicking on an element and expected a delegate call from the
     * item.
     * @param {HTMLElement} element The element to click on.
     * @param {string} callName The function expected to be called.
     * @param {Array<?>=} opt_expectedArgs The arguments the function is
     *     expected to be called with.
     */
    testClickingCalls: function(element, callName, opt_expectedArgs) {
      var mock = new MockController();
      var mockMethod = mock.createFunctionMock(this, callName);
      MockMethod.prototype.addExpectation.apply(
          mockMethod, opt_expectedArgs);
      MockInteractions.tap(element);
      mock.verifyMocks();
    },

    /** @override */
    deleteItem: function(id) {},

    /** @override */
    setItemEnabled: function(id, enabled) {},

    /** @override */
    showItemDetails: function(id) {},

    /** @override */
    setItemAllowedIncognito: function(id, enabled) {},

    /** @override */
    isInDevMode: function() { return false; },

    /** @override: */
    inspectItemView: function(id, view) {},
  };

  /** @type {string} The mock extension's id. */
  var id = 'a'.repeat(32);

  /** @type {string} The mock extension's base URL. */
  var baseUrl = 'chrome-extension://' + id + '/';

  /**
   * The data used to populate the extension item.
   * @type {ExtensionInfo}
   */
  var extensionData = {
    description: 'This is an extension',
    iconUrl: 'chrome://extension-icon/' + id + '/24/0',
    id: id,
    incognitoAccess: {isEnabled: true, isActive: false},
    name: 'Wonderful Extension',
    state: 'ENABLED',
    type: 'EXTENSION',
    version: '2.0',
    views: [{url: baseUrl + 'foo.html'}, {url: baseUrl + 'bar.html'}],
  };

  /**
   * Tests that the element's visibility matches |expectedVisible| and,
   * optionally, has specific content.
   * @param {HTMLElement} item The item to query for the element.
   * @param {boolean} expectedVisible Whether the element should be
   *     visible.
   * @param {string} selector The selector to find the element.
   * @param {string=} opt_expected The expected textContent value.
   */
  function testVisible(item, expectedVisible, selector, opt_expected) {
    var element = item.$$(selector);
    var elementIsVisible = !!element;
    // Iterate through the parents of the element (up to the item's root)
    // and check if each is visible. If one is not, then the element
    // itself is not.
    for (var e = element; elementIsVisible && e != item.shadowRoot;
         e = e.parentNode) {
      elementIsVisible = !e.hidden && e.offsetWidth > 0;
    }
    expectEquals(expectedVisible, elementIsVisible, selector);
    if (expectedVisible && opt_expected && element)
      expectEquals(opt_expected, element.textContent, selector);
  }

  // The normal elements, which should always be shown.
  var normalElements = [
    {selector: '#name', text: extensionData.name},
    {selector: '#description', text: extensionData.description},
    {selector: '#version', text: extensionData.version},
    {selector: '#enabled'},
    {selector: '#show-details'},
    {selector: '#delete-button'},
  ];
  // The elements in the details panel, which should only be shown if
  // the isShowingDetails bit is set.
  var detailElements = [
    {selector: '#allow-incognito'},
    {selector: '#details-button'},
  ];
  // The developer elements, which should only be shown if in developer
  // mode *and* showing details.
  var devElements = [
    {selector: '#extension-id', text: 'ID:' + extensionData.id},
    {selector: '#inspect-views'},
    {selector: '#inspect-views paper-button', text: 'foo.html'},
    {selector: '#inspect-views paper-button:nth-of-type(0n + 2)',
     text: 'bar.html'},
  ];

  /**
   * Tests that the elements' visibility matches the expected visibility.
   * @param {extensions.Item} item
   * @param {Array<Object<string>>} elements
   * @param {boolean} visibility
   */
  function testElementsVisibility(item, elements, visibility) {
    elements.forEach(function(element) {
      testVisible(item, visibility, element.selector, element.text);
    });
  }

  /** Tests that normal elements are visible. */
  function testNormalElementsAreVisible(item) {
    testElementsVisibility(item, normalElements, true);
  }

  /** Tests that normal elements are hidden. */
  function testNormalElementsAreHidden(item) {
    testElementsVisibility(item, normalElements, false);
  }

  /** Tests that detail elements are visible. */
  function testDetailElementsAreVisible(item) {
    testElementsVisibility(item, detailElements, true);
  }

  /** Tests that detail elements are hidden. */
  function testDetailElementsAreHidden(item) {
    testElementsVisibility(item, detailElements, false);
  }

  /** Tests that dev elements are visible. */
  function testDeveloperElementsAreVisible(item) {
    testElementsVisibility(item, devElements, true);
  }

  /** Tests that dev elements are hidden. */
  function testDeveloperElementsAreHidden(item) {
    testElementsVisibility(item, devElements, false);
  }

  var testNames = {
    ElementVisibilityNormalState: 'element visibility: normal state',
    ElementVisibilityDetailState:
        'element visibility: after tapping show details',
    ElementVisibilityDeveloperState:
        'element visibility: after enabling developer mode',
    ClickableItems: 'clickable items',
  };

  function registerTests() {
    suite('ExtensionItemTest', function() {
      /**
       * Extension item created before each test.
       * @type {extensions.Item}
       */
      var item;

      /** @type {MockDelegate} */
      var mockDelegate;

      suiteSetup(function() {
        return PolymerTest.importHtml('chrome://extensions/item.html');
      });

      // Initialize an extension item before each test.
      setup(function() {
        PolymerTest.clearBody();
        mockDelegate = new MockDelegate();
        item = new extensions.Item(extensionData, mockDelegate);
        document.body.appendChild(item);
      });

      test(assert(testNames.ElementVisibilityNormalState), function() {
        testNormalElementsAreVisible(item);
        testDetailElementsAreHidden(item);
        testDeveloperElementsAreHidden(item);

        expectTrue(item.$.enabled.checked);
        expectEquals('Enabled', item.$.enabled.textContent);
        item.set('data.state', 'DISABLED');
        expectFalse(item.$.enabled.checked);
        expectEquals('Disabled', item.$.enabled.textContent);
      });

      test(assert(testNames.ElementVisibilityDetailState), function() {
        MockInteractions.tap(item.$['show-details']);
        testNormalElementsAreVisible(item);
        testDetailElementsAreVisible(item);
        testDeveloperElementsAreHidden(item);
      });

      test(assert(testNames.ElementVisibilityDeveloperState), function() {
        MockInteractions.tap(item.$['show-details']);
        item.set('inDevMode', true);

        testNormalElementsAreVisible(item);
        testDetailElementsAreVisible(item);
        testDeveloperElementsAreVisible(item);

        // Toggling "show details" should also hide the developer elements.
        MockInteractions.tap(item.$['show-details']);
        testNormalElementsAreVisible(item);
        testDetailElementsAreHidden(item);
        testDeveloperElementsAreHidden(item);
      });

      /** Tests that the delegate methods are correctly called. */
      test(assert(testNames.ClickableItems), function() {
        MockInteractions.tap(item.$['show-details']);
        item.set('inDevMode', true);

        mockDelegate.testClickingCalls(
            item.$['delete-button'], 'deleteItem', [item.data.id]);
        mockDelegate.testClickingCalls(
            item.$.enabled, 'setItemEnabled', [item.data.id, false]);
        mockDelegate.testClickingCalls(
            item.$$('#allow-incognito'), 'setItemAllowedIncognito',
            [item.data.id, true]);
        mockDelegate.testClickingCalls(
            item.$$('#details-button'), 'showItemDetails', [item.data.id]);
        mockDelegate.testClickingCalls(
            item.$$('#inspect-views paper-button'),
            'inspectItemView', [item.data.id, item.data.views[0]]);
        mockDelegate.testClickingCalls(
            item.$$('#inspect-views paper-button:nth-of-type(0n + 2)'),
            'inspectItemView', [item.data.id, item.data.views[1]]);
      });
    });
  }

  return {
    registerTests: registerTests,
    testNames: testNames,
  };
});
