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

    activityLog.fire('view-enter-start');

    // Wait until we have finished making the call to fetch the activity log.
    return proxyDelegate.whenCalled('getExtensionActivityLog');
  });

  teardown(function() {
    activityLog.remove();
  });

  test('activities shown match search query', function() {
    const activityLogHistory = activityLog.$$('activity-log-history');
    testVisible =
        extension_test_util.testVisible.bind(null, activityLogHistory);

    const search = activityLog.$$('cr-search-field');
    assertTrue(!!search);

    // Partial, case insensitive search for i18n.getUILanguage. Whitespace is
    // also appended to the search term to test trimming.
    search.setValue('getuilanguage   ');

    return proxyDelegate.whenCalled('getFilteredExtensionActivityLog')
        .then(() => {
          Polymer.dom.flush();

          const activityLogItems =
              activityLogHistory.shadowRoot.querySelectorAll(
                  'activity-log-item');

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
              activityLogHistory.shadowRoot
                  .querySelectorAll('activity-log-item')
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
              activityLogHistory.shadowRoot.querySelectorAll(
                  'activity-log-item');
          expectEquals(activityLogItems.length, 2);
        });
  });

  test('clicking on clear activities button clears activities', function() {
    activityLog.$$('#clear-activities-button').click();

    return proxyDelegate.whenCalled('deleteActivitiesFromExtension')
        .then(() => {
          Polymer.dom.flush();
          const activityLogHistory = activityLog.$$('activity-log-history');
          testVisible =
              extension_test_util.testVisible.bind(null, activityLogHistory);

          testVisible('#no-activities', true);
          testVisible('#loading-activities', false);
          testVisible('#activity-list', false);
          expectEquals(
              activityLogHistory.shadowRoot
                  .querySelectorAll('activity-log-item')
                  .length,
              0);
        });
  });

  test('clicking on back button navigates to the details page', function() {
    Polymer.dom.flush();

    let currentPage = null;
    extensions.navigation.addListener(newPage => {
      currentPage = newPage;
    });

    activityLog.$$('#closeButton').click();
    expectDeepEquals(
        currentPage, {page: Page.DETAILS, extensionId: EXTENSION_ID});
  });

  test('tab transitions', function() {
    Polymer.dom.flush();
    // Default view should be the history view.
    testVisible('activity-log-history', true);

    // Navigate to the activity log stream.
    activityLog.$$('#real-time-tab').click();
    Polymer.dom.flush();
    testVisible('activity-log-stream', true);

    const activityLogStream = activityLog.$$('activity-log-stream');
    assertTrue(activityLogStream.isStreamOnForTest());

    // Navigate back to the activity log history tab.
    activityLog.$$('#history-tab').click();

    // Expect a refresh of the activity log.
    proxyDelegate.whenCalled('getExtensionActivityLog').then(() => {
      Polymer.dom.flush();
      testVisible('activity-log-history', true);

      // Stream should be turned off.
      assertFalse(activityLogStream.isStreamOnForTest());
    });
  });
});
