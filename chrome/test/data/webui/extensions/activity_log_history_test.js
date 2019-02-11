// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for activity-log-history. */
suite('ExtensionsActivityLogHistoryTest', function() {
  /**
   * Backing extension id, same id as the one in
   * extension_test_util.createExtensionInfo
   * @type {string}
   */
  const EXTENSION_ID = 'a'.repeat(32);

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

  /**
   * Extension activityLogHistory created before each test.
   * @type {extensions.ActivityLogHistory}
   */
  let activityLogHistory;
  let proxyDelegate;
  let testVisible;

  function setupActivityLogHistory() {
    PolymerTest.clearBody();

    activityLogHistory = new extensions.ActivityLogHistory();
    testVisible =
        extension_test_util.testVisible.bind(null, activityLogHistory);

    activityLogHistory.extensionId = EXTENSION_ID;
    activityLogHistory.lastSearch = '';
    activityLogHistory.delegate = proxyDelegate;
    document.body.appendChild(activityLogHistory);

    return proxyDelegate.whenCalled('getExtensionActivityLog');
  }

  // Initialize an extension activity log before each test.
  setup(function() {
    proxyDelegate = new extensions.TestService();
  });

  teardown(function() {
    activityLogHistory.remove();
  });

  test('activities are present for extension', function() {
    proxyDelegate.testActivities = testActivities;

    return setupActivityLogHistory().then(() => {
      Polymer.dom.flush();

      testVisible('#no-activities', false);
      testVisible('#loading-activities', false);
      testVisible('#activity-list', true);

      const activityLogItems =
          activityLogHistory.shadowRoot.querySelectorAll('activity-log-item');
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
  });

  test('script names shown for content script activities', function() {
    proxyDelegate.testActivities = testContentScriptActivities;

    return setupActivityLogHistory().then(() => {
      Polymer.dom.flush();
      const activityLogItems =
          activityLogHistory.shadowRoot.querySelectorAll('activity-log-item');

      // One activity should be shown for each content script name.
      expectEquals(activityLogItems.length, 2);

      expectEquals(
          activityLogItems[0].$$('#activity-key').innerText, 'script1.js');
      expectEquals(
          activityLogItems[1].$$('#activity-key').innerText, 'script2.js');
    });
  });

  test('other.webRequest fields shown for web request activities', function() {
    proxyDelegate.testActivities = testWebRequestActivities;

    return setupActivityLogHistory().then(() => {
      Polymer.dom.flush();
      const activityLogItems =
          activityLogHistory.shadowRoot.querySelectorAll('activity-log-item');

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

      for (let i = 0; i < expectedNumItems; ++i) {
        expectEquals(
            activityLogItems[i].$$('#activity-key').innerText,
            expectedItemKeys[i]);
      }
    });
  });

  test(
      'clicking on the delete button for an activity row deletes that row',
      function() {
        proxyDelegate.testActivities = testActivities;

        return setupActivityLogHistory().then(() => {
          Polymer.dom.flush();
          const activityLogItems =
              activityLogHistory.shadowRoot.querySelectorAll(
                  'activity-log-item');

          expectEquals(activityLogItems.length, 2);
          proxyDelegate.resetResolver('getExtensionActivityLog');
          activityLogItems[0].$$('#activity-delete-button').click();

          // We delete the first item so we should only have one item left. This
          // chaining reflects the API calls made from activity_log.js.
          return proxyDelegate.whenCalled('deleteActivitiesById')
              .then(() => proxyDelegate.whenCalled('getExtensionActivityLog'))
              .then(() => {
                Polymer.dom.flush();
                expectEquals(
                    1,
                    activityLogHistory.shadowRoot
                        .querySelectorAll('activity-log-item')
                        .length);
              });
        });
      });

  test('message shown when no activities present for extension', function() {
    // Spoof an API call and pretend that the extension has no activities.
    proxyDelegate.testActivities = {activities: []};

    return setupActivityLogHistory().then(() => {
      Polymer.dom.flush();

      testVisible('#no-activities', true);
      testVisible('#loading-activities', false);
      testVisible('#activity-list', false);
      expectEquals(
          activityLogHistory.shadowRoot.querySelectorAll('activity-log-item')
              .length,
          0);
    });
  });

  test('message shown when activities are being fetched', function() {
    // Spoof an API call and pretend that the extension has no activities.
    proxyDelegate.testActivities = {activities: []};

    return setupActivityLogHistory().then(() => {
      // Pretend the activity log is still loading.
      activityLogHistory.pageState_ = ActivityLogPageState.LOADING;

      Polymer.dom.flush();

      testVisible('#no-activities', false);
      testVisible('#loading-activities', true);
      testVisible('#activity-list', false);
    });
  });
});
