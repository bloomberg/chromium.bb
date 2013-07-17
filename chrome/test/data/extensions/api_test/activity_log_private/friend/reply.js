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
  chrome.cookies.set({
    'url': 'https://www.cnn.com',
    'name': 'activity_log_test_cookie',
    'value': 'abcdefg'
  });
  setCompleted('makeApiCall');
}

// Makes an API call that has a custom binding.
function makeSpecialApiCalls() {
  var url = chrome.extension.getURL('image/cat.jpg');
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
  var callback = function() {};
  chrome.app.getDetails();
  var b = chrome.app.isInstalled;
  var c = chrome.app.installState(callback);
  setCompleted('checkAppCalls');
}

// Makes an API call that the extension doesn't have permission for.
// Don't add the management permission or this test won't test the code path.
function makeBlockedApiCall() {
  try {
    var allExtensions = chrome.management.getAll();
  } catch (err) { }
  setCompleted('makeBlockedApiCall');
}

// Injects a content script.
function injectContentScript() {
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'file': 'google_cs.js'},
            function() {
              chrome.tabs.remove(tabId);
              setCompleted('injectContentScript');
            });
        }
      }
  );
  openTab(defaultUrl);
}

// Injects a blob of script into a page.
function injectScriptBlob() {
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': 'document.write("g o o g l e");'},
            function() {
              chrome.tabs.remove(tabId);
              setCompleted('injectScriptBlob');
            });
      }
    }
  );
  openTab(defaultUrl);
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
        // TODO(karenlees): you added this line in debugging, make sure it is
        // really needed.
        chrome.webRequest.onHeadersReceived.removeListener(doModifyHeaders);
        chrome.tabs.onUpdated.removeListener(closeTab);
        chrome.tabs.remove(tabId);
        setCompleted('doWebRequestModifications');
      }
    }
  );
  openTab(defaultUrl);
}

function getSetObjectProperties() {
  chrome.tabs.onUpdated.addListener(
    function getTabProperties(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        console.log(tab.id + ' ' + tab.index + ' ' + tab.url);
        tab.index = 3333333333333333333;
        chrome.tabs.onUpdated.removeListener(getTabProperties);
        chrome.tabs.remove(tabId);
        setCompleted('getSetObjectProperties');
      }
    }
  );
  openTab(defaultUrl);
}

function callObjectMethod() {
  var storageArea = chrome.storage.sync;
  storageArea.clear();
  setCompleted('callObjectMethod()');
}

function sendMessageToCS() {
  chrome.tabs.onUpdated.addListener(
    function messageCS(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.sendMessage(tabId, 'hellooooo!');
        chrome.tabs.onUpdated.removeListener(messageCS);
        chrome.tabs.remove(tabId);
        setCompleted('sendMessageToCS');
      }
    }
  );
  openTab(defaultUrl);
}

function sendMessageToSelf() {
  try {
    chrome.runtime.sendMessage('hello hello');
    setCompleted('sendMessageToSelf');
  } catch (err) {
    setError(err + ' in function: sendMessageToSelf');
  }
}

function sendMessageToOther() {
  try {
    chrome.runtime.sendMessage('ocacnieaapoflmkebkeaidpgfngocapl',
        'knock knock',
        function response() {
          console.log("who's there?");
        });
    setCompleted('sendMessageToOther');
  } catch (err) {
    setError(err + ' in function: sendMessageToOther');
  }
}

function connectToOther() {
  try {
    chrome.runtime.connect('ocacnieaapoflmkebkeaidpgfngocapl');
    setCompleted('connectToOther');
  } catch (err) {
    setError(err + ' in function:connectToOther');
  }
}

function tabIdTranslation() {
  var tabIds = [-1, -1];

  // Test the case of a single int
  chrome.tabs.onUpdated.addListener(
    function testSingleInt(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.executeScript(
             //tab.id,
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
        //chrome.tabs.move(tabId, {'index': -1});
        tabIds[1] = tabId;
        chrome.tabs.onUpdated.removeListener(testArray);
        chrome.tabs.remove(tabIds);
        setCompleted('tabIdTranslation');
      }
    }
  );

  openTab(defaultUrl);
}

// DOM API TEST METHODS -- PUT YOUR TESTS BELOW HERE
////////////////////////////////////////////////////////////////////////////////

// Does an XHR from this [privileged] context.
function doBackgroundXHR() {
  var request = new XMLHttpRequest();
  request.open('POST', defaultUrl, false);
  request.setRequestHeader('Content-type', 'text/plain;charset=UTF-8');
  try {
    request.send();
  } catch (err) {
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
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function() {
              chrome.tabs.remove(tabId);
              setCompleted('doContentScriptXHR');
            });
      }
    }
  );
  openTab(defaultUrl);
}

// Accesses the Location object from inside a content script.
function doLocationAccess() {
  var code = 'window.location = "http://www.google.com/#foo"; ' +
             'document.location = "http://www.google.com/#bar"; ' +
             'var loc = window.location; ' +
             'loc.assign("http://www.google.com/#fo"); ' +
             'loc.replace("http://www.google.com/#bar");';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function() {
              chrome.tabs.remove(tabId);
              setCompleted('doLocationAccess');
            });
      }
    }
  );
  openTab(defaultUrl);
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
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function() {
              chrome.tabs.remove(tabId);
              setCompleted('doDOMMutation1');
            });
      }
    }
  );
  openTab(defaultUrl);
}

function doDOMMutation2() {
  var code = 'document.write("Hello using document.write"); ' +
             'document.writeln("Hello using document.writeln"); ' +
             'document.body.innerHTML = "Hello using innerHTML";';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function() {
              chrome.tabs.remove(tabId);
              setCompleted('doDOMMutation2');
            });
      }
    }
  );
  openTab(defaultUrl);
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
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function() {
              chrome.tabs.remove(tabId);
              setCompleted('doNavigatorAPIAccess');
            });
      }
    }
  );
  openTab(defaultUrl);
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
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function() {
              chrome.tabs.remove(tabId);
              setCompleted('doWebStorageAPIAccess1');
            });
      }
    }
   );
  openTab(defaultUrl);
}

// Accesses local storage from inside a content script.
function doWebStorageAPIAccess2() {
  var code = 'var store = window.localStorage; ' +
             'store.setItem("foo", 42); ' +
             'var val = store.getItem("foo"); ' +
             'store.removeItem("foo"); ' +
             'store.clear();';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function() {
              chrome.tabs.remove(tabId);
              setCompleted('doWebStorageAPIAccess2');
            });
      }
    }
  );
  openTab(defaultUrl);
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
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
       chrome.tabs.onUpdated.removeListener(callback);
       chrome.tabs.executeScript(
           tab.id,
           {'code': code},
            function() {
              chrome.tabs.remove(tabId);
              setCompleted('doNotifcationAPIAccess');
            });
      }
    }
  );
  openTab(defaultUrl);
}

// Accesses the HTML5 ApplicationCache API from inside a content script.
function doApplicationCacheAPIAccess() {
  var code = 'var appCache = window.applicationCache;';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function() {
              chrome.tabs.remove(tabId);
              setCompleted('doApplictionCacheAPIAccess');
            });
      }
    }
  );
  openTab(defaultUrl);
}

// Accesses the HTML5 WebDatabase API from inside a content script.
function doWebDatabaseAPIAccess() {
  var code = 'var db = openDatabase("testdb", "1.0", "test database", ' +
             '                      1024 * 1024);';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function() {
              chrome.tabs.remove(tabId);
              setCompleted('doWebDatabaseAPIAccess');
            });
      }
    }
  );
  openTab(defaultUrl);
}

// Accesses the HTML5 Canvas API from inside a content script.
function doCanvasAPIAccess() {
  var code = 'var testCanvas = document.createElement("canvas"); ' +
             'var testContext = testCanvas.getContext("2d");';
  chrome.tabs.onUpdated.addListener(
    function callback(tabId, changeInfo, tab) {
      if (changeInfo['status'] === 'complete' &&
          tab.url.match(/google\.com/g)) {
        chrome.tabs.onUpdated.removeListener(callback);
        chrome.tabs.executeScript(
            tab.id,
            {'code': code},
            function() {
              chrome.tabs.remove(tabId);
              setCompleted('doCanvasAPIAccess');
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
fnMap['blocked_call'] = makeBlockedApiCall;
fnMap['inject_cs'] = injectContentScript;
fnMap['inject_blob'] = injectScriptBlob;
fnMap['webrequest'] = doWebRequestModifications;
fnMap['double'] = checkNoDoubleLogging;
fnMap['app_bindings'] = checkAppCalls;
fnMap['object_properties'] = getSetObjectProperties;
fnMap['object_methods'] = callObjectMethod;
fnMap['message_cs'] = sendMessageToCS;
fnMap['message_self'] = sendMessageToSelf;
fnMap['message_other'] = sendMessageToOther;
fnMap['connect_other'] = connectToOther;
fnMap['tab_ids'] = tabIdTranslation;
fnMap['background_xhr'] = doBackgroundXHR;
fnMap['cs_xhr'] = doContentScriptXHR;
fnMap['location_access'] = doLocationAccess;
fnMap['dom_mutation1'] = doDOMMutation1;
fnMap['dom_mutation2'] = doDOMMutation2;
fnMap['navigator_access'] = doNavigatorAPIAccess;
fnMap['web_storage_access1'] = doWebStorageAPIAccess1;
fnMap['web_storage_access2'] = doWebStorageAPIAccess2;
fnMap['notification_access'] = doNotificationAPIAccess;
fnMap['application_cache_access'] = doApplicationCacheAPIAccess;
fnMap['web_database_access'] = doWebDatabaseAPIAccess;
fnMap['canvas_access'] = doCanvasAPIAccess;

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
function setCompleted(str) {
  completed++;
  if ($('status') != null) {
    $('status').innerText = 'Completed ' + str;
  }
  console.log('[SUCCESS] ' + str);
}

function setError(str) {
  $('status').innerText = 'Error: ' + str;
}

// Set up the event listeners for use in manual run mode.
function setupEvents() {
  for (var key in fnMap) {
    if (fnMap.hasOwnProperty(key) && key != '' && $(key) != null) {
      $(key).addEventListener('click', fnMap[key]);
    }
  }
  $('incognito_checkbox').addEventListener(
      'click',
      function() { useIncognito = $('incognito_checkbox').checked; });
  setCompleted('setup events');
  completed = 0;
}
document.addEventListener('DOMContentLoaded', setupEvents);
