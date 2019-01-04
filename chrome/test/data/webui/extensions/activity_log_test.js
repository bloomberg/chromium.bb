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
        activityLogItems[0].$$('#api-call').innerText, 'i18n.getUILanguage');
    expectEquals(activityLogItems[0].$$('#activity-count').innerText, '40');

    expectEquals(
        activityLogItems[1].$$('#api-call').innerText, 'Storage.getItem');
    expectEquals(activityLogItems[1].$$('#activity-count').innerText, '35');
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
              activityLogItems[0].$$('#api-call').innerText,
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
