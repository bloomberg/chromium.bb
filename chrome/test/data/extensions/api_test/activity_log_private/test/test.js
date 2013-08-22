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
  func: function triggerObjectMethods() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'object_methods', function response() { });
  },
  expected_activity: ['storage.clear']
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
  func: function triggerBackgroundXHR() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'background_xhr', function response() { });
  },
  expected_activity: [
    'XMLHttpRequest.open',
    'XMLHttpRequest.setRequestHeader'
  ]
});
testCases.push({
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
    'tabs.move',
    'tabs.remove'
  ]
});
testCases.push({
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
    'tabs.move',
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
    'webRequest.onHeadersReceived/2',
    'webRequestInternal.eventHandled',
    'webRequest.onHeadersReceived',
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
    'webRequest.onHeadersReceived/4',
    'webRequestInternal.eventHandled',
    'webRequest.onHeadersReceived',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.remove'
  ]
});

testCases.push({
  func: function triggerApiCallsOnTabsUpdated() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'api_tab_updated', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.connect',
    'tabs.sendMessage',
    'tabs.executeScript',
    'tabs.executeScript',
    'HTMLDocument.write',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerApiCallsOnTabsUpdatedIncognito() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'api_tab_updated_incognito',
                               function response() { });
  },
  is_incognito: true,
  expected_activity: [
    'windows.create',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.connect',
    'tabs.sendMessage',
    'tabs.executeScript',
    'tabs.executeScript',
    'HTMLDocument.write',
    'tabs.remove'
  ]
});


domExpectedActivity = [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
     // Location access
    'Window.location',
    'Document.location',
    'Window.location',
    'Location.assign',
    'Location.replace',
     // Dom mutations
    'Document.createElement',
    'Document.createElement',
    'Node.appendChild',
    'Node.insertBefore',
    'Node.replaceChild',
    //'Document.location',
    'HTMLDocument.write',
    'HTMLDocument.writeln',
    'HTMLElement.innerHTML',
    // Navigator access
    'Window.navigator',
    'Geolocation.getCurrentPosition',
    'Geolocation.watchPosition',
    // Web store access - session storage
    'Window.sessionStorage',
    'Storage.setItem',
    'Storage.getItem',
    'Storage.removeItem',
    'Storage.clear',
    // Web store access - local storage
    'Window.localStorage',
    'Storage.setItem',
    'Storage.getItem',
    'Storage.removeItem',
    'Storage.clear',
    // Notification access
    'Window.webkitNotifications',
    'NotificationCenter.createNotification',
    // Cache access
    'Window.applicationCache',
    // Web database access
    'Window.openDatabase',
    // Canvas access
    'Document.createElement',
    'HTMLCanvasElement.getContext',
    // XHR from content script.
    'XMLHttpRequest.open',
    'XMLHttpRequest.setRequestHeader',
    'HTMLDocument.write'
];

// add the hook activity
hookNames = ['onclick', 'ondblclick', 'ondrag', 'ondragend', 'ondragenter',
             'ondragleave', 'ondragover', 'ondragstart', 'ondrop', 'oninput',
             'onkeydown', 'onkeypress', 'onkeyup', 'onmousedown',
             'onmouseenter', 'onmouseleave', 'onmousemove', 'onmouseout',
             'onmouseover', 'onmouseup', 'onmousewheel'];

for (var i = 0; i < hookNames.length; i++) {
  domExpectedActivity.push('Element.' + hookNames[i]);
  domExpectedActivity.push('Document.' + hookNames[i]);
  domExpectedActivity.push('Window.' + hookNames[i]);
}

// Close the tab.
domExpectedActivity.push('tabs.remove');

testCases.push({
  func: function triggerDOMChangesOnTabsUpdated() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'dom_tab_updated', function response() { });
  },
  expected_activity: domExpectedActivity
});

// copy the array for the next test so we can modify it
var domExpectedActivityIncognito = domExpectedActivity.slice(0);

// put windows.create at the front of the expected values for the next test
domExpectedActivityIncognito.unshift('windows.create');

testCases.push({
  func: function triggerDOMChangesOnTabsUpdated() {
    chrome.runtime.sendMessage('pknkgggnfecklokoggaggchhaebkajji',
                               'dom_tab_updated_incognito',
                               function response() { });
  },
  is_incognito: true,
  expected_activity: domExpectedActivityIncognito
});

// Listener to check the expected logging is done in the test cases.
var testCaseIndx = 0;
var callIndx = -1;
chrome.activityLogPrivate.onExtensionActivity.addListener(
    function(activity) {
      var activityId = activity['extensionId'];
      chrome.test.assertEq('pknkgggnfecklokoggaggchhaebkajji', activityId);

      // Check the api call is the one we expected next.
      var apiCall = activity['apiCall'];
      expectedCall = 'runtime.onMessageExternal';
      if (callIndx > -1) {
        expectedCall = testCases[testCaseIndx].expected_activity[callIndx];
      }
      console.log('Logged:' + apiCall + ' Expected:' + expectedCall);
      chrome.test.assertEq(expectedCall, apiCall);

      // Check that no real URLs are logged in incognito-mode tests.
      // TODO(felt): This isn't working correctly. crbug.com/272920
      /*if (activity['activityType'] == 'dom_access') {
        var url = activity['pageUrl'];
        if (url) {
          if (testCases[testCaseIndx].is_incognito) {
            chrome.test.assertEq('<incognito>', url,
                                 'URL was not anonymized for apiCall:' +
                                 apiCall);
          } else {
            chrome.test.assertTrue(url != '<incognito>',
                                   'Non-incognito URL was anonymized');
          }
        }
      }*/

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
