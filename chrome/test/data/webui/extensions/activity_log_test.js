// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-activity-log. */
suite('ExtensionsActivityLogTest', function() {
  /**
   * Backing extension id, same id as the one in
   * extension_test_util.createExtensionInfo
   * @type {string}
   */
  const EXTENSION_ID = 'a'.repeat(32);

  /**
   * Extension activityLog created before each test.
   * @type {extensions.ActivityLog}
   */
  let activityLog;
  let proxyDelegate;
  let testVisible;

  const testActivities = {
    activities: [
      {
        activityId: '299',
        activityType: 'api_call',
        apiCall: 'i18n.getUILanguage',
        args: 'null',
        count: 10,
        extensionId: EXTENSION_ID,
        time: 1541203132002.664
      },
      {
        activityId: '309',
        activityType: 'dom_access',
        apiCall: 'Storage.getItem',
        args: 'null',
        count: 35,
        extensionId: EXTENSION_ID,
        other: {domVerb: 'method'},
        pageTitle: 'Test Extension',
        pageUrl: `chrome-extension://${EXTENSION_ID}/index.html`,
        time: 1541203131994.837
      },
      {
        activityId: '301',
        activityType: 'api_call',
        apiCall: 'i18n.getUILanguage',
        args: 'null',
        count: 30,
        extensionId: EXTENSION_ID,
        time: 1541203172002.664
      },
    ]
  };

  // Sample activities representing content script invocations. Activities with
  // missing args will not be processed.
  const testContentScriptActivities = {
    activities: [
      {
        activityId: '288',
        activityType: 'content_script',
        apiCall: '',
        args: `["script1.js","script2.js"]`,
        count: 1,
        extensionId: EXTENSION_ID,
        pageTitle: 'Test Extension',
        pageUrl: 'https://www.google.com/search'
      },
      {
        activityId: '290',
        activityType: 'content_script',
        apiCall: '',
        count: 1,
        extensionId: EXTENSION_ID,
        pageTitle: 'Test Extension',
        pageUrl: 'https://www.google.com/search'
      },
    ]
  };

  // Sample activities representing web requests. Activities with valid fields
  // in other.webRequest should be split into multiple entries; one for every
  // field. Activities with empty fields will have the group name be just the
  // web request API call.
  const testWebRequestActivities = {
    activities: [
      {
        activityId: '1337',
        activityType: 'web_request',
        apiCall: 'webRequest.onBeforeSendHeaders',
        args: 'null',
        count: 300,
        extensionId: EXTENSION_ID,
        other: {
          webRequest:
              `{"modified_request_headers":true, "added_request_headers":"a"}`
        },
        pageUrl: `chrome-extension://${EXTENSION_ID}/index.html`,
        time: 1546499283237.616
      },
      {
        activityId: '1339',
        activityType: 'web_request',
        apiCall: 'webRequest.noWebRequestObject',
        args: 'null',
        count: 3,
        extensionId: EXTENSION_ID,
        other: {},
        pageUrl: `chrome-extension://${EXTENSION_ID}/index.html`,
        time: 1546499283237.616
      },
    ]
  };

  // Initialize an extension activity log before each test.
  setup(function() {
    PolymerTest.clearBody();

    activityLog = new extensions.ActivityLog();
    testVisible = extension_test_util.testVisible.bind(null, activityLog);

    activityLog.extensionId = EXTENSION_ID;
    proxyDelegate = new extensions.TestService();
    activityLog.delegate = proxyDelegate;
    proxyDelegate.testActivities = testActivities;
    document.body.appendChild(activityLog);

    // Wait until we have finished making the call to fetch the activity log.
    return proxyDelegate.whenCalled('getExtensionActivityLog');
  });

  teardown(function() {
    activityLog.remove();
  });

  test('activities are present for extension', function() {
    Polymer.dom.flush();

    testVisible('#no-activities', false);
    testVisible('#loading-activities', false);
    testVisible('#activity-list', true);

    const activityLogItems =
        activityLog.shadowRoot.querySelectorAll('activity-log-item');
    expectEquals(activityLogItems.length, 2);

    // Test the order of the activity log items here. This test is in this
    // file because the logic to group activity log items by their API call
    // is in activity_log.js.
    expectEquals(
        activityLogItems[0].$$('#activity-key').innerText,
        'i18n.getUILanguage');
    expectEquals(activityLogItems[0].$$('#activity-count').innerText, '40');

    expectEquals(
        activityLogItems[1].$$('#activity-key').innerText, 'Storage.getItem');
    expectEquals(activityLogItems[1].$$('#activity-count').innerText, '35');
  });

  test('script names shown for content script activities', function() {
    proxyDelegate.resetResolver('getExtensionActivityLog');
    proxyDelegate.testActivities = testContentScriptActivities;

    activityLog.refreshActivities().then(() => {
      Polymer.dom.flush();
      const activityLogItems =
          activityLog.shadowRoot.querySelectorAll('activity-log-item');

      // One activity should be shown for each content script name.
      expectEquals(activityLogItems.length, 2);

      expectEquals(
          activityLogItems[0].$$('#activity-key').innerText, 'script1.js');
      expectEquals(
          activityLogItems[1].$$('#activity-key').innerText, 'script2.js');
    });
  });

  test('other.webRequest fields shown for web request activities', function() {
    proxyDelegate.resetResolver('getExtensionActivityLog');
    proxyDelegate.testActivities = testWebRequestActivities;

    activityLog.refreshActivities().then(() => {
      Polymer.dom.flush();
      const activityLogItems =
          activityLog.shadowRoot.querySelectorAll('activity-log-item');

      // First activity should be split into two groups as it has two actions
      // recorded in the other.webRequest object. We display the names of these
      // actions along with the API call. Second activity should fall back
      // to using just the API call as the key. Hence we end up with three
      // activity log items.
      const expectedItemKeys = [
        'webRequest.onBeforeSendHeaders (added_request_headers)',
        'webRequest.onBeforeSendHeaders (modified_request_headers)',
        'webRequest.noWebRequestObject'
      ];
      const expectedNumItems = expectedItemKeys.length;

      expectEquals(activityLogItems.length, expectedNumItems);

      for (let idx = 0; idx < expectedNumItems; ++idx) {
        expectEquals(
            activityLogItems[idx].$$('#activity-key').innerText,
            expectedItemKeys[idx]);
      }
    });
  });

  test('activities shown match search query', function() {
    const search = activityLog.$$('cr-search-field');
    assertTrue(!!search);

    // Partial, case insensitive search for i18n.getUILanguage. Whitespace is
    // also appended to the search term to test trimming.
    search.setValue('getuilanguage   ');

    return proxyDelegate.whenCalled('getFilteredExtensionActivityLog')
        .then(() => {
          Polymer.dom.flush();

          const activityLogItems =
              activityLog.shadowRoot.querySelectorAll('activity-log-item');

          // Since we searched for an API call, we expect only one match as
          // activity log entries are grouped by their API call.
          expectEquals(activityLogItems.length, 1);
          expectEquals(
              activityLogItems[0].$$('#activity-key').innerText,
              'i18n.getUILanguage');

          // Change search query so no results match.
          proxyDelegate.resetResolver('getFilteredExtensionActivityLog');
          search.setValue('query that does not match any activities');

          return proxyDelegate.whenCalled('getFilteredExtensionActivityLog');
        })
        .then(() => {
          Polymer.dom.flush();

          testVisible('#no-activities', true);
          testVisible('#loading-activities', false);
          testVisible('#activity-list', false);

          expectEquals(
              activityLog.shadowRoot.querySelectorAll('activity-log-item')
                  .length,
              0);

          proxyDelegate.resetResolver('getExtensionActivityLog');

          // Finally, we clear the search query via the #clearSearch button. We
          // should see all the activities displayed.
          search.$$('#clearSearch').click();
          return proxyDelegate.whenCalled('getExtensionActivityLog');
        })
        .then(() => {
          Polymer.dom.flush();

          const activityLogItems =
              activityLog.shadowRoot.querySelectorAll('activity-log-item');
          expectEquals(activityLogItems.length, 2);
        });
  });

  test('clicking on clear activities button clears activities', function() {
    activityLog.$$('#clear-activities-button').click();

    return proxyDelegate.whenCalled('deleteActivitiesFromExtension')
        .then(() => {
          Polymer.dom.flush();
          testVisible('#no-activities', true);
          testVisible('#loading-activities', false);
          testVisible('#activity-list', false);
          expectEquals(
              activityLog.shadowRoot.querySelectorAll('activity-log-item')
                  .length,
              0);
        });
  });

  test('message shown when no activities present for extension', function() {
    // Spoof an API call and pretend that the extension has no activities.
    activityLog.activityData_ = [];

    Polymer.dom.flush();

    testVisible('#no-activities', true);
    testVisible('#loading-activities', false);
    testVisible('#activity-list', false);
    expectEquals(
        activityLog.shadowRoot.querySelectorAll('activity-log-item').length, 0);
  });

  test('message shown when activities are being fetched', function() {
    // Pretend the activity log is still loading.
    activityLog.pageState_ = ActivityLogPageState.LOADING;

    Polymer.dom.flush();

    testVisible('#no-activities', false);
    testVisible('#loading-activities', true);
    testVisible('#activity-list', false);
  });

  test('clicking on back button navigates to the details page', function() {
    Polymer.dom.flush();

    let currentPage = null;
    extensions.navigation.addListener(newPage => {
      currentPage = newPage;
    });

    activityLog.$$('#close-button').click();
    expectDeepEquals(
        currentPage, {page: Page.DETAILS, extensionId: EXTENSION_ID});
  });
});
