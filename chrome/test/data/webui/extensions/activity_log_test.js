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

  /**
   * Backing extension info for the activity log.
   * @type {chrome.developerPrivate.ExtensionInfo}
   */
  let extensionInfo;

  let proxyDelegate;
  let testVisible;

  const testActivities = {activities: []};

  // Initialize an extension activity log before each test.
  setup(function() {
    PolymerTest.clearBody();

    activityLog = new extensions.ActivityLog();
    testVisible = extension_test_util.testVisible.bind(null, activityLog);

    extensionInfo = extension_test_util.createExtensionInfo({
      id: EXTENSION_ID,
    });
    activityLog.extensionInfo = extensionInfo;

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

    // Navigate back to the activity log history tab.
    activityLog.$$('#history-tab').click();

    // Expect a refresh of the activity log.
    proxyDelegate.whenCalled('getExtensionActivityLog').then(() => {
      Polymer.dom.flush();
      testVisible('activity-log-history', true);
    });
  });
});
