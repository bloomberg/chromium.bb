// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-detail-view. */
cr.define('extension_detail_view_tests', function() {
  /** @enum {string} */
  var TestNames = {
    Layout: 'layout',
    ClickableElements: 'clickable elements',
  };

  function registerTests() {
    suite('ExtensionItemTest', function() {
      /**
       * Extension item created before each test.
       * @type {extensions.Item}
       */
      var item;

      /**
       * Backing extension data for the item.
       * @type {chrome.developerPrivate.ExtensionInfo}
       */
      var extensionData;

      /** @type {extension_test_util.MockItemDelegate} */
      var mockDelegate;

      suiteSetup(function() {
        return PolymerTest.importHtml('chrome://extensions/item.html');
      });

      // Initialize an extension item before each test.
      setup(function() {
        PolymerTest.clearBody();
        extensionData = extension_test_util.createExtensionInfo({
          incognitoAccess: {isEnabled: true, isActive: false},
          fileAccess: {isEnabled: true, isActive: false},
          runOnAllUrls: {isEnabled: true, isActive: false},
          errorCollection: {isEnabled: true, isActive: false}
        });
        mockDelegate = new extension_test_util.MockItemDelegate();
        item = new extensions.DetailView();
        item.set('data', extensionData);
        item.set('delegate', mockDelegate);
        document.body.appendChild(item);
      });

      test(assert(TestNames.Layout), function() {
        Polymer.dom.flush();

        extension_test_util.testIronIcons(item);

        var testIsVisible = extension_test_util.isVisible.bind(null, item);
        expectTrue(testIsVisible('#close-button'));
        expectTrue(testIsVisible('#open-in-webstore'));
        expectTrue(testIsVisible('#options'));

        // Check the checkboxes visibility and state. They should be visible
        // only if the associated option is enabled, and checked if the
        // associated option is active.
        var accessOptions = [
          {key: 'incognitoAccess', id: '#allow-incognito'},
          {key: 'fileAccess', id: '#allow-on-file-urls'},
          {key: 'runOnAllUrls', id: '#allow-on-all-sites'},
          {key: 'errorCollection', id: '#collect-errors'},
        ];
        var isChecked = function(id) {
          var el = item.$$(id);
          assert(el, id);
          return el.hasAttribute('checked');
        };
        for (let option of accessOptions) {
          expectTrue(extension_test_util.isVisible(item, option.id));
          expectFalse(isChecked(option.id), option.id);
          item.set('data.' + option.key + '.isEnabled', false);
          Polymer.dom.flush();
          expectFalse(extension_test_util.isVisible(item, option.id));
          item.set('data.' + option.key + '.isEnabled', true);
          item.set('data.' + option.key + '.isActive', true);
          Polymer.dom.flush();
          expectTrue(extension_test_util.isVisible(item, option.id));
          expectTrue(isChecked(option.id));
        }

        expectFalse(testIsVisible('#dependent-extensions-list'));
        item.set('data.dependentExtensions',
                 [{id: 'aaa', name: 'Dependent1'},
                  {id: 'bbb', name: 'Dependent2'}]);
        Polymer.dom.flush();
        expectTrue(testIsVisible('#dependent-extensions-list'));
        expectEquals(2,
                     item.$$('#dependent-extensions-list').querySelectorAll(
                         'li').length);
      });

      test(assert(TestNames.ClickableElements), function() {
        Polymer.dom.flush();
        mockDelegate.testClickingCalls(
            item.$$('#allow-incognito'), 'setItemAllowedIncognito',
            [extensionData.id, true]);
        mockDelegate.testClickingCalls(
            item.$$('#allow-on-file-urls'), 'setItemAllowedOnFileUrls',
            [extensionData.id, true]);
        mockDelegate.testClickingCalls(
            item.$$('#allow-on-all-sites'), 'setItemAllowedOnAllSites',
            [extensionData.id, true]);
        mockDelegate.testClickingCalls(
            item.$$('#collect-errors'), 'setItemCollectsErrors',
            [extensionData.id, true]);
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
