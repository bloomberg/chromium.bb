// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
  * Every test needs:
  *   - a button in options.html
  *   - a function that runs the test & calls setCompleted when done
  *   - a listener registered in setupEvents
**/

// UTILITY METHODS
////////////////////////////////////////////////////////////////////////////////

var robot = false;
var completed = total = 0;

// Lets us know that we're running in the test suite and should notify the
// browser about the test status.
function setRunningAsRobot() {
  robot = true;
}

// Convenience.
function $(o) {
  return document.getElementById(o);
}

// Constructs and returns a function to remove a given tab. These functions
// are used as callbacks in calls to chrome.tabs.executeScript.
function removeTabCallbackMaker(tabId) {
    return function() { chrome.tabs.remove(tabId); };
}

// Track how many tests have finished.
function setCompleted(str) {
  completed++;
  $('status').innerText = "Completed " + str;
  console.log("[SUCCESS] " + str);
  if (robot && completed == total)
    chrome.test.notifyPass();
}

// TEST METHODS -- PUT YOUR TESTS BELOW HERE
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

// Makes an API call that the extension doesn't have permission for.
function makeBlockedApiCall() {
  try {
    var all_extensions = chrome.management.getAll();
  } catch(err) { }
  setCompleted('makeBlockedApiCall');
}

// Injects a content script.
function injectContentScript() {
  chrome.tabs.onUpdated.addListener(
    function injCS(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" && tab.url.match(/google\.fr/g)) {
        chrome.tabs.executeScript(tab.id, {'file': 'google_cs.js'},
                                  removeTabCallbackMaker(tabId));
        chrome.tabs.onUpdated.removeListener(injCS);
        setCompleted('injectContentScript');
      }
    }
  );
  window.open('http://www.google.fr');
}

// Injects a blob of script into a page.
function injectScriptBlob() {
  chrome.tabs.onUpdated.addListener(
    function injSB(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete"
          && tab.url.match(/google\.com/g)) {
        chrome.tabs.executeScript(tab.id,
                                  {'code': 'document.write("g o o g l e");'},
                                  removeTabCallbackMaker(tabId));
        chrome.tabs.onUpdated.removeListener(injSB);
        setCompleted('injectScriptBlob');
      }
    }
  );
  window.open('http://www.google.com');
}

// Does an XHR from this [privileged] context.
function doBackgroundXHR() {
  var request = new XMLHttpRequest();
  request.open('GET', 'http://www.google.com', false);
  try {
    request.send();
  } catch(err) {
    // doesn't matter if it works or not; should be recorded either way
  }
  setCompleted('doBackgroundXHR');
}

// Does an XHR from inside a content script.
function doContentScriptXHR() {
  var xhr = 'var request = new XMLHttpRequest(); ' +
            'request.open("GET", "http://www.cnn.com", false); ' +
            'request.send(); ' +
            'document.write("sent an XHR");';
  chrome.tabs.onUpdated.addListener(
    function doCSXHR(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" && tab.url.match(/google\.cn/g)) {
        chrome.tabs.executeScript(tab.id, {'code': xhr},
                                  removeTabCallbackMaker(tabId));
        chrome.tabs.onUpdated.removeListener(doCSXHR);
        setCompleted('doContentScriptXHR');
      }
    }
  );
  window.open('http://www.google.cn');
}

// Modifies the headers sent and received in an HTTP request using the
// webRequest API.
function doWebRequestModifications() {
  // Install a webRequest handler that will add an HTTP header to the outgoing
  // request for the main page.
  function doModifyHeaders(details) {
    var headers = details.requestHeaders;
    if (headers === undefined) {
      headers = [];
    }
    headers.push({'name': 'X-Test-Activity-Log-Send',
                  'value': 'Present'});
    return {'requestHeaders': headers};
  }
  chrome.webRequest.onBeforeSendHeaders.addListener(
      doModifyHeaders,
      {'urls': ['http://*/*'], 'types': ['main_frame']},
      ['blocking', 'requestHeaders']);

  // Open a tab, then close it when it has finished loading--this should give
  // the webRequest handler a chance to run.
  chrome.tabs.onUpdated.addListener(
    function closeTab(tabId, changeInfo, tab) {
      if (changeInfo['status'] === "complete" &&
          tab.url.match(/google\.co\.uk/g)) {
        chrome.webRequest.onBeforeSendHeaders.removeListener(doModifyHeaders);
        chrome.tabs.onUpdated.removeListener(closeTab);
        chrome.tabs.remove(tabId);
        setCompleted('doWebRequestModifications');
      }
    }
  );
  window.open('http://www.google.co.uk');
}

// REGISTER YOUR TESTS HERE
// Attach the tests to buttons.
function setupEvents() {
  $('api_call').addEventListener('click', makeApiCall);
  $('special_call').addEventListener('click', makeSpecialApiCalls);
  $('blocked_call').addEventListener('click', makeBlockedApiCall);
  $('inject_cs').addEventListener('click', injectContentScript);
  $('inject_blob').addEventListener('click', injectScriptBlob);
  $('background_xhr').addEventListener('click', doBackgroundXHR);
  $('cs_xhr').addEventListener('click', doContentScriptXHR);
  $('webrequest').addEventListener('click', doWebRequestModifications);
  $('double').addEventListener('click', checkNoDoubleLogging);

  completed = 0;
  total = document.getElementsByTagName('button').length;
}

document.addEventListener('DOMContentLoaded', setupEvents);

