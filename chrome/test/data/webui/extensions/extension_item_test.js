// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-item. */
cr.define('extension_item_tests', function() {
  /**
   * The data used to populate the extension item.
   * @type {chrome.developerPrivate.ExtensionInfo}
   */
  var extensionData = extension_test_util.createExtensionInfo();

  // The normal elements, which should always be shown.
  var normalElements = [
    {selector: '#name', text: extensionData.name},
    {selector: '#icon'},
    {selector: '#description', text: extensionData.description},
    {selector: '#enable-toggle'},
    {selector: '#details-button'},
    {selector: '#remove-button'},
  ];
  // The developer elements, which should only be shown if in developer
  // mode *and* showing details.
  var devElements = [
    {selector: '#version', text: extensionData.version},
    {selector: '#extension-id', text: 'ID:' + extensionData.id},
    {selector: '#inspect-views'},
    {selector: '#inspect-views a[is="action-link"]', text: 'foo.html,'},
    {selector: '#inspect-views a[is="action-link"]:nth-of-type(2)',
     text: '1 more…'},
  ];

  /**
   * Tests that the elements' visibility matches the expected visibility.
   * @param {extensions.Item} item
   * @param {Array<Object<string>>} elements
   * @param {boolean} visibility
   */
  function testElementsVisibility(item, elements, visibility) {
    elements.forEach(function(element) {
      extension_test_util.testVisible(
          item, element.selector, visibility, element.text);
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

  /** Tests that dev elements are visible. */
  function testDeveloperElementsAreVisible(item) {
    testElementsVisibility(item, devElements, true);
  }

  /** Tests that dev elements are hidden. */
  function testDeveloperElementsAreHidden(item) {
    testElementsVisibility(item, devElements, false);
  }

  /** @enum {string} */
  var TestNames = {
    ElementVisibilityNormalState: 'element visibility: normal state',
    ElementVisibilityDeveloperState:
        'element visibility: after enabling developer mode',
    ClickableItems: 'clickable items',
    Warnings: 'warnings',
    SourceIndicator: 'source indicator',
  };

  function registerTests() {
    suite('ExtensionItemTest', function() {
      /**
       * Extension item created before each test.
       * @type {extensions.Item}
       */
      var item;

      /** @type {extension_test_util.MockItemDelegate} */
      var mockDelegate;

      suiteSetup(function() {
        return PolymerTest.importHtml('chrome://extensions/item.html');
      });

      // Initialize an extension item before each test.
      setup(function() {
        PolymerTest.clearBody();
        mockDelegate = new extension_test_util.MockItemDelegate();
        item = new extensions.Item();
        item.set('data', extensionData);
        item.set('delegate', mockDelegate);
        document.body.appendChild(item);
      });

      test(assert(TestNames.ElementVisibilityNormalState), function() {
        testNormalElementsAreVisible(item);
        testDeveloperElementsAreHidden(item);

        expectTrue(item.$['enable-toggle'].checked);
        item.set('data.state', 'DISABLED');
        expectFalse(item.$['enable-toggle'].checked);
      });

      test(assert(TestNames.ElementVisibilityDeveloperState), function() {
        item.set('inDevMode', true);

        testNormalElementsAreVisible(item);
        testDeveloperElementsAreVisible(item);

        extension_test_util.testVisible(item, '#dev-reload-button', false);
        item.set('data.location', chrome.developerPrivate.Location.UNPACKED);
        Polymer.dom.flush();
        extension_test_util.testVisible(item, '#dev-reload-button', true);
      });

      /** Tests that the delegate methods are correctly called. */
      test(assert(TestNames.ClickableItems), function() {
        item.set('inDevMode', true);

        mockDelegate.testClickingCalls(
            item.$['remove-button'], 'deleteItem', [item.data.id]);
        mockDelegate.testClickingCalls(
            item.$['enable-toggle'], 'setItemEnabled', [item.data.id, false]);
        mockDelegate.testClickingCalls(
            item.$$('#inspect-views a[is="action-link"]'),
            'inspectItemView', [item.data.id, item.data.views[0]]);

        var listener1 = new extension_test_util.ListenerMock();
        listener1.addListener(item, 'extension-item-show-details',
                             {data: item.data});
        MockInteractions.tap(item.$$('#details-button'));
        listener1.verify();

        var listener2 = new extension_test_util.ListenerMock();
        listener2.addListener(item, 'extension-item-show-details',
                             {data: item.data});
        MockInteractions.tap(
            item.$$('#inspect-views a[is="action-link"]:nth-of-type(2)'));
        listener2.verify();

        item.set('data.disableReasons.corruptInstall', true);
        Polymer.dom.flush();
        mockDelegate.testClickingCalls(
            item.$$('#repair-button'), 'repairItem', [item.data.id]);

        item.set('data.disableReasons.corruptInstall', false);
        Polymer.dom.flush();
        mockDelegate.testClickingCalls(
            item.$$('#terminated-reload-button'), 'reloadItem', [item.data.id]);

        item.set('data.location', chrome.developerPrivate.Location.UNPACKED);
        Polymer.dom.flush();
        mockDelegate.testClickingCalls(
            item.$$('#dev-reload-button'), 'reloadItem', [item.data.id]);
      });

      test(assert(TestNames.Warnings), function() {
        var hasCorruptedWarning = function() {
          return extension_test_util.isVisible(item, '#corrupted-warning');
        };
        var hasSuspiciousWarning = function() {
          return extension_test_util.isVisible(item, '#suspicious-warning');
        };
        var hasBlacklistedWarning = function() {
          return extension_test_util.isVisible(item, '#blacklisted-warning');
        };

        expectFalse(hasCorruptedWarning());
        expectFalse(hasSuspiciousWarning());
        expectFalse(hasBlacklistedWarning());

        item.set('data.disableReasons.corruptInstall', true);
        Polymer.dom.flush();
        expectTrue(hasCorruptedWarning());
        expectFalse(hasSuspiciousWarning());
        expectFalse(hasBlacklistedWarning());

        item.set('data.disableReasons.suspiciousInstall', true);
        Polymer.dom.flush();
        expectTrue(hasCorruptedWarning());
        expectTrue(hasSuspiciousWarning());
        expectFalse(hasBlacklistedWarning());

        item.set('data.blacklistText', 'This item is blacklisted');
        Polymer.dom.flush();
        expectTrue(hasCorruptedWarning());
        expectTrue(hasSuspiciousWarning());
        expectTrue(hasBlacklistedWarning());

        item.set('data.blacklistText', undefined);
        Polymer.dom.flush();
        expectTrue(hasCorruptedWarning());
        expectTrue(hasSuspiciousWarning());
        expectFalse(hasBlacklistedWarning());

        item.set('data.disableReasons.corruptInstall', false);
        item.set('data.disableReasons.suspiciousInstall', false);
        Polymer.dom.flush();
        expectFalse(hasCorruptedWarning());
        expectFalse(hasSuspiciousWarning());
        expectFalse(hasBlacklistedWarning());
      });

      test(assert(TestNames.SourceIndicator), function() {
        expectFalse(extension_test_util.isVisible(item, '#source-indicator'));
        item.set('data.location', 'UNPACKED');
        Polymer.dom.flush()
        expectTrue(extension_test_util.isVisible(item, '#source-indicator'));
        var icon = item.$$('#source-indicator iron-icon');
        assertTrue(!!icon);
        expectEquals('extensions-icons:unpacked', icon.icon);
        extension_test_util.testIronIcons(item);

        item.set('data.location', 'THIRD_PARTY');
        Polymer.dom.flush();
        expectTrue(extension_test_util.isVisible(item, '#source-indicator'));
        expectEquals('input', icon.icon);
        extension_test_util.testIronIcons(item);

        item.set('data.location', 'FROM_STORE');
        item.set('data.controlledInfo', {type: 'POLICY', text: 'policy'});
        Polymer.dom.flush();
        expectTrue(extension_test_util.isVisible(item, '#source-indicator'));
        expectEquals('communication:business', icon.icon);
        extension_test_util.testIronIcons(item);

        item.set('data.controlledInfo', null);
        Polymer.dom.flush();
        expectFalse(extension_test_util.isVisible(item, '#source-indicator'));
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
