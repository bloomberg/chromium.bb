// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
  * Every test needs:
  *   - a button in options.html
  *   - a function that runs the test & calls setCompletedChrome or
        setCompetedDOM when done
  *   - a listener registered in setupEvents
**/

// UTILITY METHODS
////////////////////////////////////////////////////////////////////////////////

var robot = false;
var completed = 0;
var testButtons = [];
var defaultUrl = 'http://www.google.com';
// Lets us know that we're running in the test suite and should notify the
// browser about the test status.
function setRunningAsRobot() {
  robot = true;
}

// set the testButtons array to the current set of test cases.
function setTestButtons(buttonsArray) {
  testButtons = buttonsArray;
}

// Clicks the first button from the array 'testButtons'. If robot is true
// then the next test button automatically gets clicked on once
// 'setCompleted' gets called. ('setCompleted' gets invoked
// when a test completes successfully.)
function beginClickingTestButtons() {
  if (testButtons.length > 0) {
    completed = 0;
    testButtons[0].click();
  } else {
    console.log("testButtons array is empty, somehting is wrong");
  }
}

// Convenience.
function $(o) {
  return document.getElementById(o);
}

// Track how many tests have finished. If there are pending tests,
// then automatically trigger them by clicking the next test button
// from the array 'testButtons'.
function setCompleted(str) {
  completed++;
  $('status').innerText = "Completed " + str;
  console.log("[SUCCESS] " + str);
  if (robot) {
    if (completed === testButtons.length) {
      // Done with clicking all buttons in the array 'testButtons'.
      chrome.test.notifyPass();
    } else {
      // Click the next button from the array 'testButtons'.
      testButtons[completed].click();
    }
  }
}

// CHROME API TEST METHODS -- PUT YOUR TESTS BELOW HERE
////////////////////////////////////////////////////////////////////////////////

// Makes an API call.
function makeApiCall() {
  chrome.cookies.set({
    'url': 'https://www.cnn.com',
    'name': 'activity_log_test_cookie',
    'value': 'abcdefg'
  });
  setCompleted('makeApiCall');
}

// Makes an API call that has a custom binding.
function makeSpecialApiCalls() {
  var url = chrome.extension.getURL("image/cat.jpg");
  var noparam = chrome.extension.getViews();
  setCompleted('makeSpecialApiCalls');
}

// Checks that we don't double-log calls that go through setHandleRequest
// *and* the ExtensionFunction machinery.
function checkNoDoubleLogging() {
  chrome.omnibox.setDefaultSuggestion({description: 'hello world'});
  setCompleted('checkNoDoubleLogging');
}

// Check whether we log calls to chrome.app.*;
function checkAppCalls() {
  var callback = function () {};
  chrome.app.getDetails();
  var b = chrome.app.isInstalled;
  var c = chrome.app.installState(callback);
  setCompleted('checkAppCalls');
}

// Makes an API call that the extension doesn't have permission for.
// Don't add the management permission or this test won't test the code path.
function makeBlockedApiCall() {
  try {
    var all_extensions = chrome.management.getAll();
  } catch(err) { }
  setCompleted('makeBlockedApiCall');
}

// Injects a content script.
function injectContentScript() {
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'file': 'google_cs.js'},
            function () {
              chrome.tabs.remove(tabId);
              setCompleted('injectContentScript');
            });
      }
    }
  );
  window.open(defaultUrl);
}

// Injects a blob of script into a page.
function injectScriptBlob() {
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete"
          && tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': 'document.write("g o o g l e");'},
            function () {
              chrome.tabs.remove(tabId);
              setCompleted('injectScriptBlob');
            });
      }
    }
  );
  window.open(defaultUrl);
}

// Modifies the headers sent and received in an HTTP request using the
// webRequest API.
function doWebRequestModifications() {
  // Install a webRequest handler that will add an HTTP header to the outgoing
  // request for the main page.
  function doModifyHeaders(details) {
    var response = {};

    var headers = details.requestHeaders;
    if (headers === undefined) {
      headers = [];
    }
    headers.push({'name': 'X-Test-Activity-Log-Send',
                  'value': 'Present'});
    response['requestHeaders'] = headers;

    headers = details.responseHeaders;
    if (headers === undefined) {
      headers = [];
    }
    headers = headers.filter(
        function(x) {return x["name"] != "Cache-Control"});
    headers.push({'name': 'X-Test-Response-Header',
                  'value': 'Inserted'});
    headers.push({'name': 'Set-Cookie',
                  'value': 'ActivityLog=InsertedCookie'});
    response['responseHeaders'] = headers;

    return response;
  }
  chrome.webRequest.onBeforeSendHeaders.addListener(
      doModifyHeaders,
      {'urls': ['http://*/*'], 'types': ['main_frame']},
      ['blocking', 'requestHeaders']);
  chrome.webRequest.onHeadersReceived.addListener(
      doModifyHeaders,
      {'urls': ['http://*/*'], 'types': ['main_frame']},
      ['blocking', 'responseHeaders']);

  // Open a tab, then close it when it has finished loading--this should give
  // the webRequest handler a chance to run.
  chrome.tabs.onUpdated.addListener(
    function closeTab(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.com/g)) {
        chrome.webRequest.onBeforeSendHeaders.removeListener(doModifyHeaders);
        chrome.tabs.onUpdated.removeListener(closeTab);
        chrome.tabs.remove(tabId);
        setCompleted('doWebRequestModifications');
      }
    }
  );
  window.open(defaultUrl);
}

function getSetObjectProperties() {
  chrome.tabs.onUpdated.addListener(
    function getTabProperties(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete"
          && tab.url.match(/google\.com/g)) {
        console.log(tab.id + " " + tab.index + " " + tab.url);
        tab.index = 3333333333333333333;
        chrome.tabs.remove(tabId);
        chrome.tabs.onUpdated.removeListener(getTabProperties);
        setCompleted('getSetObjectProperties');
      }
    }
  );
  window.open(defaultUrl);
}

function callObjectMethod() {
  var storageArea = chrome.storage.sync;
  storageArea.clear();
  setCompleted('callObjectMethod()');
}

function sendMessageToCS() {
  chrome.tabs.onUpdated.addListener(
    function messageCS(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete"
          && tab.url.match(/google\.com/g)) {
        chrome.tabs.sendMessage(tabId, "hellooooo!");
        chrome.tabs.remove(tabId);
        chrome.tabs.onUpdated.removeListener(messageCS);
        setCompleted('sendMessageToCS');
      }
    }
  );
  window.open(defaultUrl);
}

function sendMessageToSelf() {
  chrome.runtime.sendMessage("hello hello");
  setCompleted('sendMessageToSelf');
}

function sendMessageToOther() {
  chrome.runtime.sendMessage("ocacnieaapoflmkebkeaidpgfngocapl",
                             "knock knock",
                             function response() {
                               console.log("who's there?");
                             });
  setCompleted('sendMessageToOther');
}

function connectToOther() {
  chrome.runtime.connect("ocacnieaapoflmkebkeaidpgfngocapl");
  setCompleted('connectToOther');
}

function tabIdTranslation() {
  var tabIds = [-1, -1];

  // Test the case of a single int
  chrome.tabs.onUpdated.addListener(
    function testSingleInt(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.executeScript(
            tab.id,
            {'file': 'google_cs.js'},
            function() {
              chrome.tabs.onUpdated.removeListener(testSingleInt);
              tabIds[0] = tabId;
              window.open('http://www.google.be');
            });
      }
    }
  );

  // Test the case of arrays
  chrome.tabs.onUpdated.addListener(
    function testArray(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" && tab.url.match(/google\.be/g)) {
        chrome.tabs.move(tabId, {"index": -1});
        tabIds[1] = tabId;
        chrome.tabs.remove(tabIds);
        chrome.tabs.onUpdated.removeListener(testArray);
        setCompleted('tabIdTranslation');
      }
    }
  );

  window.open(defaultUrl);
}

// DOM API TEST METHODS -- PUT YOUR TESTS BELOW HERE
////////////////////////////////////////////////////////////////////////////////

// Does an XHR from this [privileged] context.
function doBackgroundXHR() {
  var request = new XMLHttpRequest();
  request.open('POST', defaultUrl, false);
  request.setRequestHeader("Content-type", "text/plain;charset=UTF-8");
  try {
    request.send();
  } catch(err) {
    // doesn't matter if it works or not; should be recorded either way
  }
  setCompleted('doBackgroundXHR');
}

// Does an XHR from inside a content script.
function doContentScriptXHR() {
  var code = 'var request = new XMLHttpRequest(); ' +
             'request.open("POST", "http://www.cnn.com", false); ' +
             'request.setRequestHeader("Content-type", ' +
             '                         "text/plain;charset=UTF-8"); ' +
             'request.send(); ' +
             'document.write("sent an XHR");';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function () {
              chrome.tabs.remove(tabId);
              setCompleted('doContentScriptXHR');
            });
      }
    }
  );
  window.open(defaultUrl);
}

// Accesses the Location object from inside a content script.
function doLocationAccess() {
  var code = 'window.location = "http://www.google.com/#foo"; ' +
             'document.location = "http://www.google.com/#bar"; ' +
             'var loc = window.location; ' +
             'loc.assign("http://www.google.com/#foo"); ' +
             'loc.replace("http://www.google.com/#bar");';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function () {
              chrome.tabs.remove(tabId);
              setCompleted('doLoctionAccess');
            });
      }
    }
  );
  window.open(defaultUrl);
}

// Mutates the DOM tree from inside a content script.
function doDOMMutation1() {
  var code = 'var d1 = document.createElement("div"); ' +
             'var d2 = document.createElement("div"); ' +
             'document.body.appendChild(d1); ' +
             'document.body.insertBefore(d2, d1); ' +
             'document.body.replaceChild(d1, d2);';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function () {
              chrome.tabs.remove(tabId);
              setCompleted('doDOMMutation1');
            });
      }
    }
  );
  window.open(defaultUrl);
}

function doDOMMutation2() {
  var code = 'document.write("Hello using document.write"); ' +
             'document.writeln("Hello using document.writeln"); ' +
             'document.body.innerHTML = "Hello using innerHTML";';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function () {
              chrome.tabs.remove(tabId);
              setCompleted('doDOMMutation2');
            });
      }
    }
  );
  window.open(defaultUrl);
}

// Accesses the HTML5 Navigator API from inside a content script.
function doNavigatorAPIAccess() {
  var code = 'var geo = navigator.geolocation; ' +
             'var successCallback = function(x) { }; ' +
             'var errorCallback = function(x) { }; ' +
             'geo.getCurrentPosition(successCallback, errorCallback); ';
             'var id = geo.watchPosition(successCallback, errorCallback);';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function () {
              chrome.tabs.remove(tabId);
              setCompleted('doNavigatorAPIAccess');
            });
      }
    }
  );
  window.open(defaultUrl);
}

// Accesses the HTML5 WebStorage API from inside a content script.
function doWebStorageAPIAccess1() {
  var code = 'var store = window.sessionStorage; ' +
             'store.setItem("foo", 42); ' +
             'var val = store.getItem("foo"); ' +
             'store.removeItem("foo"); ' +
             'store.clear();';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function () {
              chrome.tabs.remove(tabId);
              setCompleted('doWebStorageAPIAccess1');
            });
      }
    }
   );
  window.open(defaultUrl);
}

function doWebStorageAPIAccess2() {
  var code = 'var store = window.localStorage; ' +
             'store.setItem("foo", 42); ' +
             'var val = store.getItem("foo"); ' +
             'store.removeItem("foo"); ' +
             'store.clear();';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function () {
              chrome.tabs.remove(tabId);
              setCompleted('doWebStorageAPIAccess2');
            });
      }
    }
  );
  window.open(defaultUrl);
}

// Accesses the HTML5 Notification API from inside a content script.
function doNotificationAPIAccess() {
  var code = 'try {' +
             '  webkitNotifications.createNotification("myIcon.png", ' +
             '                                         "myTitle", ' +
             '                                         "myContent");' +
             '} catch (e) {}';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.com/g)) {
       chrome.tabs.onUpdated.removeListener(callback);
       chrome.tabs.executeScript(
           tab.id,
           {'code': code},
            function () {
              chrome.tabs.remove(tabId);
              setCompleted('doNotifcationAPIAccess');
            });
      }
    }
  );
  window.open(defaultUrl);
}

// Accesses the HTML5 ApplicationCache API from inside a content script.
function doApplicationCacheAPIAccess() {
  var code = 'var appCache = window.applicationCache;';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function () {
              chrome.tabs.remove(tabId);
              setCompleted('doApplictionCacheAPIAccess');
            });
      }
    }
  );
  window.open(defaultUrl);
}

// Accesses the HTML5 WebDatabase API from inside a content script.
function doWebDatabaseAPIAccess() {
  var code = 'var db = openDatabase("testdb", "1.0", "test database", ' +
             '                      1024 * 1024);';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function () {
              chrome.tabs.remove(tabId);
              setCompleted('doWebDatabaseAPIAccess');
            });
      }
    }
  );
  window.open(defaultUrl);
}

// Accesses the HTML5 Canvas API from inside a content script.
function doCanvasAPIAccess() {
  var code = 'var test_canvas = document.createElement("canvas"); ' +
             'var test_context = test_canvas.getContext("2d");';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function () {
              chrome.tabs.remove(tabId);
              setCompleted('doCanvasAPIAccess');
            });
      }
    }
  );
  window.open(defaultUrl);
}

// REGISTER YOUR TESTS HERE
// Attach the tests to buttons.
function setupEvents() {
  $('api_call').addEventListener('click', makeApiCall);
  $('special_call').addEventListener('click', makeSpecialApiCalls);
  $('blocked_call').addEventListener('click', makeBlockedApiCall);
  $('inject_cs').addEventListener('click', injectContentScript);
  $('inject_blob').addEventListener('click', injectScriptBlob);
  $('webrequest').addEventListener('click', doWebRequestModifications);
  $('double').addEventListener('click', checkNoDoubleLogging);
  $('app_bindings').addEventListener('click', checkAppCalls);
  $('object_properties').addEventListener('click', getSetObjectProperties);
  $('object_methods').addEventListener('click', callObjectMethod);
  $('message_cs').addEventListener('click', sendMessageToCS);
  $('message_self').addEventListener('click', sendMessageToSelf);
  $('message_other').addEventListener('click', sendMessageToOther);
  $('connect_other').addEventListener('click', connectToOther);
  $('tab_ids').addEventListener('click', tabIdTranslation);
  $('background_xhr').addEventListener('click', doBackgroundXHR);
  $('cs_xhr').addEventListener('click', doContentScriptXHR);
  $('location_access').addEventListener('click', doLocationAccess);
  $('dom_mutation1').addEventListener('click', doDOMMutation1);
  $('dom_mutation2').addEventListener('click', doDOMMutation2);
  $('navigator_access').addEventListener('click', doNavigatorAPIAccess);
  $('web_storage_access1').addEventListener('click',
                                            doWebStorageAPIAccess1);
  $('web_storage_access2').addEventListener('click',
                                            doWebStorageAPIAccess2);
  $('notification_access').addEventListener('click', doNotificationAPIAccess);
  $('application_cache_access').addEventListener(
      'click',
      doApplicationCacheAPIAccess);
  $('web_database_access').addEventListener('click', doWebDatabaseAPIAccess);
  $('canvas_access').addEventListener('click', doCanvasAPIAccess);
  completed = 0;
}

document.addEventListener('DOMContentLoaded', setupEvents);

