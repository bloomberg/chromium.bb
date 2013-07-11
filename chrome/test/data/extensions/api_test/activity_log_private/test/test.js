// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Setup the test cases.
var testCases = [];
testCases.push({
  func: function triggerApiCall() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'api_call', function response() { });
  },
  expected_activity: ['cookies.set']
});
testCases.push({
  func: function triggerSpecialCall() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'special_call', function response() { });
  },
  expected_activity: [
    'extension.getURL',
    'extension.getViews'
  ]
});
testCases.push({
  func: function triggerBlockedCall() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'blocked_call', function response() { });
   },
   expected_activity: []
});
testCases.push({
  func: function triggerInjectCS() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'inject_cs', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerInjectCSIncognito() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'inject_cs_incognito', function response() { });
  },
  is_incognito: true,
  expected_activity: [
    'windows.create',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerInsertBlob() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'inject_blob', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.remove']
});
testCases.push({
  func: function triggerDouble() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'double', function response() {});
  },
  expected_activity: ['omnibox.setDefaultSuggestion']
});
testCases.push({
  func: function triggerAppBindings() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'app_bindings', function response() { });
  },
  expected_activity: [
    'app.GetDetails',
    'app.GetIsInstalled',
    'app.getInstallState'
  ]
});
testCases.push({
  func: function triggerObjectProperties() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'object_properties', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.remove']
});
testCases.push({
  func: function triggerObjectMethods() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'object_methods', function response() { });
  },
  expected_activity: ['storage.clear']
});
testCases.push({
  func: function triggerMessageCS() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'message_cs', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.connect',
    'tabs.sendMessage',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerMessageSelf() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'message_self', function response() { });
  },
  expected_activity: [
    'runtime.connect',
    'runtime.sendMessage'
  ]
});
testCases.push({
  func: function triggerMessageOther() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'message_other', function response() { });
  },
  expected_activity: [
    'runtime.connect',
    'runtime.sendMessage'
  ]
});
testCases.push({
  func: function triggerConnectOther() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'connect_other', function response() { });
  },
  expected_activity: ['runtime.connect']
});
testCases.push({
  func: function triggerLocationAccess() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'location_access', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerDomMutation1() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'dom_mutation1', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerDomMutation2() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'dom_mutation2', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerNavigatorAccess() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'navigator_access', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerWebStorageAccess1() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'web_storage_access1', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerWebStorageAccess2() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'web_storage_access2', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerNotificationAccess() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'notification_access', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerApplicationCacheAccess() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'application_cache_access',
                               function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerWebDatabaseAccess() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'web_database_access',
                               function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerCanvasAccess() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'canvas_access', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.remove'
  ]
});
testCases.push({
  name: 'tab_ids',
  func: function triggerTabIds() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'tab_ids', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.remove'
  ]
});
testCases.push({
  name: 'tab_ids_incognito',
  func: function triggerTabIdsIncognito() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'tab_ids_incognito', function response() { });
  },
  is_incognito: true,
  expected_activity: [
    'windows.create',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'windows.create',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.remove'
  ]
});

testCases.push({
  func: function triggerWebRequest() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'webrequest', function response() { });
  },
  expected_activity: [
    'webRequestInternal.addEventListener',
    'webRequestInternal.addEventListener',
    'webRequest.onBeforeSendHeaders/1',
    'webRequestInternal.eventHandled',
    'webRequest.onBeforeSendHeaders',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.remove'
  ]
});

testCases.push({
  func: function triggerWebRequestIncognito() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'webrequest_incognito', function response() { });
  },
  is_incognito: true,
  expected_activity: [
    'webRequestInternal.addEventListener',
    'webRequestInternal.addEventListener',
    'windows.create',
    'webRequest.onBeforeSendHeaders/3',
    'webRequestInternal.eventHandled',
    'webRequest.onBeforeSendHeaders',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.remove'
  ]
});

testCases.push({
  func: function triggerBackgroundXHR() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'cs_xhr', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.remove'
  ]
});

testCases.push({
  func: function triggerBackgroundXHR() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'background_xhr', function response() { });
  },
  expected_activity: [
    'XMLHttpRequest.open',
    'XMLHttpRequest.setRequestHeader'
  ]
});

// Listener to check the expected logging is done in the test cases.
var testCaseIndx = 0;
var callIndx = -1;
chrome.activityLogPrivate.onExtensionActivity.addListener(
    function(activity) {
      var activityId = activity['extensionId'];
      chrome.test.assertEq('pknkgggnfecklokoggaggchhaebkajji', activityId);

      // Get the api call info from either the chrome activity or dom activity.
      var activityType = activity['activityType'];
      var activityDetailName = 'chromeActivityDetail';
      if (activity['activityType'] == 'dom') {
        activityDetailName = 'domActivityDetail';
      }

      // Check the api call is the one we expected next.
      var apiCall = activity[activityDetailName]['apiCall'];
      expectedCall = 'runtime.onMessageExternal';
      if (callIndx > -1) {
        expectedCall = testCases[testCaseIndx].expected_activity[callIndx];
      }
      console.log('Logged:' + apiCall + ' Expected:' + expectedCall);
      chrome.test.assertEq(expectedCall, apiCall);

      // Check that no real URLs are logged in incognito-mode tests.
      if (activity['activityType'] == 'dom') {
        var url = activity[activityDetailName]['url'];
        if (url) {
          if (testCases[testCaseIndx].is_incognito) {
            chrome.test.assertEq('http://incognito/', url);
          } else {
            chrome.test.assertTrue(url != 'http://incognito/',
                                   'Non-incognito URL was anonymized');
          }
        }
      }

      // If all the expected calls have been logged for this test case then
      // mark as suceeded and move to the next. Otherwise look for the next
      // expected api call.
      if (callIndx == testCases[testCaseIndx].expected_activity.length - 1) {
        chrome.test.succeed();
        callIndx = -1;
        testCaseIndx++;
      } else {
        callIndx++;
      }
    }
);

function getTestCasesToRun() {
  var tests = [];
  for (var i = 0; i < testCases.length; i++) {
    if (testCases[i].func != undefined) {
      tests.push(testCases[i].func);
    }
  }
  return tests;
}
chrome.test.runTests(getTestCasesToRun());
