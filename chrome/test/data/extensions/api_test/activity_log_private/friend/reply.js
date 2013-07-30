// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var defaultUrl = 'http://www.google.com';

// Utility function to open a URL in a new tab.  If the useIncognito global is
// true, the URL is opened in a new incognito window, otherwise it is opened in
// a new tab in the current window.  Alternatively, whether to use incognito
// can be specified as a second argument which overrides the global setting.
var useIncognito = false;
function openTab(url, incognito) {
  if (incognito == undefined ? useIncognito : incognito) {
    chrome.windows.create({'url': url, 'incognito': true});
  } else {
    window.open(url);
  }
}

// CHROME API TEST METHODS -- PUT YOUR TESTS BELOW HERE
////////////////////////////////////////////////////////////////////////////////

// Makes an API call.
function makeApiCall() {
  resetStatus();
  chrome.cookies.set({
    'url': 'https://www.cnn.com',
    'name': 'activity_log_test_cookie',
    'value': 'abcdefg'
  });
  appendCompleted('makeApiCall');
}

// Makes an API call that has a custom binding.
function makeSpecialApiCalls() {
  resetStatus();
  var url = chrome.extension.getURL('image/cat.jpg');
  var noparam = chrome.extension.getViews();
  appendCompleted('makeSpecialApiCalls');
}

// Checks that we don't double-log calls that go through setHandleRequest
// *and* the ExtensionFunction machinery.
function checkNoDoubleLogging() {
  resetStatus();
  chrome.omnibox.setDefaultSuggestion({description: 'hello world'});
  appendCompleted('checkNoDoubleLogging');
}

// Check whether we log calls to chrome.app.*;
function checkAppCalls() {
  resetStatus();
  var callback = function() {};
  chrome.app.getDetails();
  var b = chrome.app.isInstalled;
  var c = chrome.app.installState(callback);
  appendCompleted('checkAppCalls');
}

// Makes an API call that the extension doesn't have permission for.
// Don't add the management permission or this test won't test the code path.
function makeBlockedApiCall() {
  resetStatus();
  try {
    var allExtensions = chrome.management.getAll();
  } catch (err) { }
  appendCompleted('makeBlockedApiCall');
}

function callObjectMethod() {
  resetStatus();
  var storageArea = chrome.storage.sync;
  storageArea.clear();
  appendCompleted('callObjectMethod');
}

// Modifies the headers sent and received in an HTTP request using the
// webRequest API.
function doWebRequestModifications() {
  resetStatus();
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
        function(x) {return x['name'] != 'Cache-Control'});
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
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.webRequest.onBeforeSendHeaders.removeListener(doModifyHeaders);
        chrome.webRequest.onHeadersReceived.removeListener(doModifyHeaders);
        chrome.tabs.onUpdated.removeListener(closeTab);
        chrome.tabs.remove(tabId);
        appendCompleted('doWebRequestModifications');
      }
    }
  );
  openTab(defaultUrl);
}

function sendMessageToSelf() {
  resetStatus();
  try {
    chrome.runtime.sendMessage('hello hello');
    appendCompleted('sendMessageToSelf');
  } catch (err) {
    setError(err + ' in function: sendMessageToSelf');
  }
}

function sendMessageToOther() {
  resetStatus();
  try {
    chrome.runtime.sendMessage('ocacnieaapoflmkebkeaidpgfngocapl',
        'knock knock',
        function response() {
          console.log("who's there?");
          appendCompleted('sendMessageToOther');
        });
  } catch (err) {
    setError(err + ' in function: sendMessageToOther');
  }
}

function connectToOther() {
  resetStatus();
  try {
    chrome.runtime.connect('ocacnieaapoflmkebkeaidpgfngocapl');
    appendCompleted('connectToOther');
  } catch (err) {
    setError(err + ' in function:connectToOther');
  }
}

function tabIdTranslation() {
  resetStatus();
  var tabIds = [-1, -1];

  // Test the case of a single int
  chrome.tabs.onUpdated.addListener(
    function testSingleInt(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.executeScript(
            tabId,
            {'file': 'google_cs.js'},
            function() {
              chrome.tabs.onUpdated.removeListener(testSingleInt);
              tabIds[0] = tabId;
              openTab('http://www.google.be');
            });
      }
    }
  );

  // Test the case of arrays
  chrome.tabs.onUpdated.addListener(
    function testArray(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' && tab.url.match(/google\.be/g)) {
        chrome.tabs.move(tabId, {'index': -1});
        tabIds[1] = tabId;
        chrome.tabs.onUpdated.removeListener(testArray);
        chrome.tabs.remove(tabIds);
        appendCompleted('tabIdTranslation');
      }
    }
  );

  openTab(defaultUrl);
}

function executeApiCallsOnTabUpdated() {
  resetStatus();
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);

        // Send a message.
        chrome.tabs.sendMessage(tabId, 'hellooooo!');
        appendCompleted('sendMessageToCS');

        // Inject a content script
        chrome.tabs.executeScript(
            tab.id,
            {'file': 'google_cs.js'},
            function() {
              appendCompleted('injectContentScript');
            });

        // Injects a blob of script into a page and cleans up the tab when
        // finished.
        chrome.tabs.executeScript(
            tab.id,
            {'code': 'document.write("g o o g l e");'},
            function() {
              appendCompleted('injectScriptBlob');
              chrome.tabs.remove(tabId);
            });
      }
    }
  );
  openTab(defaultUrl);
}


// DOM API TEST METHODS -- PUT YOUR TESTS BELOW HERE
////////////////////////////////////////////////////////////////////////////////

// Does an XHR from this [privileged] context.
function doBackgroundXHR() {
  resetStatus();
  var request = new XMLHttpRequest();
  request.open('POST', defaultUrl, false);
  request.setRequestHeader('Content-type', 'text/plain;charset=UTF-8');
  try {
    request.send();
  } catch (err) {
    // doesn't matter if it works or not; should be recorded either way
  }
  appendCompleted('doBackgroundXHR');
}

function executeDOMChangesOnTabUpdated() {
  resetStatus();
  code = '';

  // Accesses the Location object from inside a content script.
  code = 'window.location = "http://www.google.com/#foo"; ' +
          'document.location = "http://www.google.com/#bar"; ' +
          'var loc = window.location; ' +
          'loc.assign("http://www.google.com/#fo"); ' +
          'loc.replace("http://www.google.com/#bar");';

  // Mutates the DOM tree from inside a content script.
  code += 'var d1 = document.createElement("div"); ' +
          'var d2 = document.createElement("div"); ' +
          'document.body.appendChild(d1); ' +
          'document.body.insertBefore(d2, d1); ' +
          'document.body.replaceChild(d1, d2);';

  code += 'document.write("Hello using document.write"); ' +
          'document.writeln("Hello using document.writeln"); ' +
          'document.body.innerHTML = "Hello using innerHTML";';

  // Accesses the HTML5 Navigator API from inside a content script.
  code += 'var geo = navigator.geolocation; ' +
          'var successCallback = function(x) { }; ' +
          'var errorCallback = function(x) { }; ' +
          'geo.getCurrentPosition(successCallback, errorCallback); ' +
          'var id = geo.watchPosition(successCallback, errorCallback);';

  // Accesses the HTML5 WebStorage API from inside a content script.
  code += 'var store = window.sessionStorage; ' +
          'store.setItem("foo", 42); ' +
          'var val = store.getItem("foo"); ' +
          'store.removeItem("foo"); ' +
          'store.clear();';

  // Same but for localStorage.
  code += 'var store = window.localStorage; ' +
          'store.setItem("foo", 42); ' +
          'var val = store.getItem("foo"); ' +
          'store.removeItem("foo"); ' +
          'store.clear();';

  // Accesses the HTML5 Notification API from inside a content script.
  code += 'try {' +
          '  webkitNotifications.createNotification("myIcon.png", ' +
          '                                         "myTitle", ' +
          '                                         "myContent");' +
          '} catch (e) {}';

  // Accesses the HTML5 ApplicationCache API from inside a content script.
  code += 'var appCache = window.applicationCache;';

  // Accesses the HTML5 WebDatabase API from inside a content script.
  code += 'var db = openDatabase("testdb", "1.0", "test database", ' +
          '                      1024 * 1024);';

  // Accesses the HTML5 Canvas API from inside a content script.
  code += 'var testCanvas = document.createElement("canvas"); ' +
          'var testContext = testCanvas.getContext("2d");';

  // Does an XHR from inside a content script.
  code += 'var request = new XMLHttpRequest(); ' +
          'request.open("POST", "http://www.cnn.com", false); ' +
          'request.setRequestHeader("Content-type", ' +
          '                         "text/plain;charset=UTF-8"); ' +
          'request.send(); ' +
          'document.write("sent an XHR");';

  // This function is used as a handler for hooking mouse and keyboard events.
  code += 'function handlerHook(event) { };';

  hookNames = ['onclick', 'ondblclick', 'ondrag', 'ondragend', 'ondragenter',
               'ondragleave', 'ondragover', 'ondragstart', 'ondrop', 'oninput',
               'onkeydown', 'onkeypress', 'onkeyup', 'onmousedown',
               'onmouseenter', 'onmouseleave', 'onmousemove', 'onmouseout',
               'onmouseover', 'onmouseup', 'onmousewheel'];

  // Access to each hook can be monitored for Element, Document, and Window.
  for (var i = 0; i < hookNames.length; i++) {
    // handler on Element
    code += 'document.body.' + hookNames[i] + ' = handlerHook;';

    // handler on a Document
    code += 'document.' + hookNames[i] + ' = handlerHook;';

    // handler on a Window
    code += 'window.' + hookNames[i] + ' = handlerHook;';
  }

  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
             tabId, {'code': code},
             function() {
               chrome.tabs.remove(tabId);
               appendCompleted('executeDOMChangesOnTabUpdated');
             });
      }
    }
  );
  openTab(defaultUrl);
}

// ADD TESTS CASES TO THE MAP HERE.
var fnMap = {};
fnMap['api_call'] = makeApiCall;
fnMap['special_call'] = makeSpecialApiCalls;
fnMap['double'] = checkNoDoubleLogging;
fnMap['app_bindings'] = checkAppCalls;
fnMap['blocked_call'] = makeBlockedApiCall;
fnMap['object_methods'] = callObjectMethod;
fnMap['message_self'] = sendMessageToSelf;
fnMap['message_other'] = sendMessageToOther;
fnMap['connect_other'] = connectToOther;
fnMap['background_xhr'] = doBackgroundXHR;
fnMap['webrequest'] = doWebRequestModifications;
fnMap['tab_ids'] = tabIdTranslation;
fnMap['dom_tab_updated'] = executeDOMChangesOnTabUpdated;
fnMap['api_tab_updated'] = executeApiCallsOnTabUpdated;

// Setup function mapping for the automated tests.
try {
  chrome.runtime.onMessageExternal.addListener(
      function(message, sender, response) {
        useIncognito = false;
        if (message.match(/_incognito$/)) {
          // Enable incognito windows for this test, then strip the _incognito
          // suffix for the lookup below.
          useIncognito = true;
          message = message.slice(0, -10);
        }
        if (fnMap.hasOwnProperty(message)) {
          fnMap[message]();
        } else {
          console.log('UNKNOWN METHOD: ' + message);
        }
      }
      );
} catch (err) {
  console.log('Error while adding listeners: ' + err);
}

// Convenience functions for the manual run mode.
function $(o) {
  return document.getElementById(o);
}

var completed = 0;
function resetStatus(str) {
  completed = 0;
  if ($('status') != null) {
    $('status').innerText = '';
  }
}

function appendCompleted(str) {
  if ($('status') != null) {
    if (completed > 0) {
      $('status').innerText += ', ' + str;
    } else {
      $('status').innerText = 'Completed: ' + str;
    }
  }
  completed += 1;
  console.log('Completed ' + str);
}

function appendError(str) {
  if ($('status') != null) {
    $('status').innerText += 'Error: ' + str;
  }
}

// Set up the event listeners for use in manual run mode.
function setupEvents() {
  for (var key in fnMap) {
    if (fnMap.hasOwnProperty(key) && key != '' && $(key) != null) {
      $(key).addEventListener('click', fnMap[key]);
    }
  }
  if ($('incognito_checkbox') != null) {
    $('incognito_checkbox').addEventListener(
        'click',
        function() { useIncognito = $('incognito_checkbox').checked; });
  }
  completed = 0;
  appendCompleted('setup events');
}
document.addEventListener('DOMContentLoaded', setupEvents);
