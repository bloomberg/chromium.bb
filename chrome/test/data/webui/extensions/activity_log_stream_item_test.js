// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for activity-log-stream-item. */
suite('ExtensionsActivityLogStreamItemTest', function() {
  /**
   * Extension activityLogStreamItem created before each test.
   * @type {extensions.ActivityLogStreamItem}
   */
  let activityLogStreamItem;
  let testVisible;

  /**
   * StreamItem data for the activityLogStreamItem
   * @type {extensions.StreamItem}
   */
  let testStreamItem;

  // Initialize an activity log stream item before each test.
  setup(function() {
    PolymerTest.clearBody();
    testStreamItem = {
      id: 'testAPI.testMethod1550101623113',
      name: 'testAPI.testMethod',
      timestamp: 1550101623113,
      activityType: chrome.activityLogPrivate.ExtensionActivityType.API_CALL,
      pageUrl: '',
      args: JSON.stringify([]),
      expanded: false
    };

    activityLogStreamItem = new extensions.ActivityLogStreamItem();
    activityLogStreamItem.data = testStreamItem;
    testVisible =
        extension_test_util.testVisible.bind(null, activityLogStreamItem);

    document.body.appendChild(activityLogStreamItem);
  });

  test(
      'item not expandable if it has no page URL, args or web request info',
      function() {
        Polymer.dom.flush();

        testVisible('cr-expand-button', true);

        // Since |cr-expand-button| is always visible, we test that the
        // |cr-icon-button| within is not visible.
        testVisible('cr-icon-button', false);
      });

  test(
      'page URL, args and web request info visible when item is expanded',
      function() {
        testStreamItem = {
          id: 'testAPI.testMethod1550101623113',
          name: 'testAPI.testMethod',
          timestamp: 1550101623113,
          activityType:
              chrome.activityLogPrivate.ExtensionActivityType.API_CALL,
          pageUrl: 'example.url',
          args: JSON.stringify([null]),
          webRequestInfo: 'web request info',
          expanded: false
        };

        activityLogStreamItem.set('data', testStreamItem);

        Polymer.dom.flush();
        testVisible('cr-expand-button', true);
        activityLogStreamItem.$$('cr-expand-button').click();

        Polymer.dom.flush();
        testVisible('#page-url-link', true);
        testVisible('#args-list', true);
        testVisible('#web-request-section', true);
      });

  test('placeholder arg values are replaced by the argUrl', function() {
    const argUrl = 'arg.url';
    const placeholder = extensions.ARG_URL_PLACEHOLDER;
    // The <arg_url> placeholder except the '<' is escaped into a unicode
    // string to simulate the serializer on the C++ side.
    const escapedPlaceholder = '\\u003Carg_url>';

    testStreamItem = {
      id: 'testAPI.testMethod1550101623113',
      name: 'testAPI.testMethod',
      timestamp: 1550101623113,
      activityType: chrome.activityLogPrivate.ExtensionActivityType.API_CALL,
      argUrl: argUrl,
      args: `[
        "${placeholder}",
        "${escapedPlaceholder}",
        ["${placeholder}"],
        {"url":"${escapedPlaceholder}"}
      ]`,
      expanded: false
    };

    activityLogStreamItem.set('data', testStreamItem);

    Polymer.dom.flush();
    testVisible('cr-expand-button', true);
    activityLogStreamItem.$$('cr-expand-button').click();

    Polymer.dom.flush();
    testVisible('#args-list', true);

    const argsDisplayed =
        activityLogStreamItem.shadowRoot.querySelectorAll('.arg');
    expectEquals(4, argsDisplayed.length);

    expectEquals(`"${argUrl}"`, argsDisplayed[0].innerText);
    expectEquals(`"${argUrl}"`, argsDisplayed[1].innerText);
    expectEquals(`["${argUrl}"]`, argsDisplayed[2].innerText);
    expectEquals(`{"url":"${argUrl}"}`, argsDisplayed[3].innerText);
  });

  teardown(function() {
    activityLogStreamItem.remove();
  });
});
