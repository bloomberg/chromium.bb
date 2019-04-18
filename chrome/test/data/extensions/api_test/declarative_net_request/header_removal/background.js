// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Navigates to |url| and invokes |callback| when the navigation is complete.
function navigateTab(url, callback) {
  chrome.tabs.onUpdated.addListener(function updateCallback(_, info, tab) {
    if (info.status == 'complete' && tab.url == url) {
      chrome.tabs.onUpdated.removeListener(updateCallback);
      callback(tab);
    }
  });

  chrome.tabs.update({url: url});
}

var testServerPort;
var host = 'xyz.com';
function getServerURL(path) {
  if (!testServerPort)
    throw new Error('Called getServerURL outside of runTests.');
  return `http://${host}:${testServerPort}/${path}`
}

// Returns whether |headerName| is present in |headers|.
function checkHasHeader(headers, headerName) {
  return !!headers.find(header => header.name.toLowerCase() == headerName);
}

// Checks whether the cookie request header was removed from the request and
// that it isn't visible to web request listeners. Then proceeds to the next
// test.
function checkCookieHeaderRemoved(expectRemoved) {
  var echoCookieUrl = getServerURL('echoheader?cookie');

  // Register web request listeners for |echoCookieUrl|.
  var filter = {urls: [echoCookieUrl]};
  var extraInfoSpec = ['requestHeaders', 'extraHeaders'];
  var onBeforeSendHeadersSeen = false;
  chrome.webRequest.onBeforeSendHeaders.addListener(function listener(details) {
    chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
    onBeforeSendHeadersSeen = true;
    chrome.test.assertEq(
        !expectRemoved, checkHasHeader(details.requestHeaders, 'cookie'));
  }, filter, extraInfoSpec);

  var onSendHeadersSeen = false;
  chrome.webRequest.onSendHeaders.addListener(function listener(details) {
    chrome.webRequest.onSendHeaders.removeListener(listener);
    onSendHeadersSeen = true;
    chrome.test.assertEq(
        !expectRemoved, checkHasHeader(details.requestHeaders, 'cookie'));
  }, filter, extraInfoSpec);

  navigateTab(echoCookieUrl, function(tab) {
    chrome.test.assertTrue(onBeforeSendHeadersSeen);
    chrome.test.assertTrue(onSendHeadersSeen);
    chrome.tabs.executeScript(
        tab.id, {code: 'document.body.innerText'}, function(results) {
          chrome.test.assertNoLastError();
          chrome.test.assertEq(
              expectRemoved ? 'None' : 'foo1=bar1; foo2=bar2', results[0]);
          chrome.test.succeed();
        });
  });
}

// Checks whether the set-cookie response header was removed from the request
// and that it isn't visible to web request listeners. Then proceeds to the next
// test.
function checkSetCookieHeaderRemoved(expectRemoved) {
  var setCookieUrl = getServerURL('set-cookie?foo1=bar1&foo2=bar2');

  // Register web request listeners for |setCookieUrl|.
  var filter = {urls: [setCookieUrl]};
  var extraInfoSpec = ['responseHeaders', 'extraHeaders'];
  var onHeadersReceivedSeen = false;
  chrome.webRequest.onHeadersReceived.addListener(function listener(details) {
    chrome.webRequest.onHeadersReceived.removeListener(listener);
    onHeadersReceivedSeen = true;
    chrome.test.assertEq(
        !expectRemoved, checkHasHeader(details.responseHeaders, 'set-cookie'));
  }, filter, extraInfoSpec);

  var onResponseStartedSeen = false;
  chrome.webRequest.onResponseStarted.addListener(function listener(details) {
    chrome.webRequest.onResponseStarted.removeListener(listener);
    onResponseStartedSeen = true;
    chrome.test.assertEq(
        !expectRemoved, checkHasHeader(details.responseHeaders, 'set-cookie'));
  }, filter, extraInfoSpec);

  var removeCookiesPromise =
      function(cookieParams) {
    return new Promise((resolve, reject) => {
      chrome.cookies.remove(cookieParams, function(details) {
        chrome.test.assertNoLastError();
        resolve();
      });
    });
  }

  var removeCookie1 =
      removeCookiesPromise({url: getServerURL(''), 'name': 'foo1'});
  var removeCookie2 =
      removeCookiesPromise({url: getServerURL(''), 'name': 'foo2'});

  // Clear cookies from existing tests.
  Promise.all([removeCookie1, removeCookie2]).then(function() {
    navigateTab(setCookieUrl, function(tab) {
      chrome.test.assertTrue(onHeadersReceivedSeen);
      chrome.test.assertTrue(onResponseStartedSeen);
      chrome.cookies.getAll({url: getServerURL('')}, function(cookies) {
        if (expectRemoved) {
          chrome.test.assertEq(0, cookies.length);
        } else {
          chrome.test.assertEq(2, cookies.length);
          chrome.test.assertTrue(
              !!cookies.find(cookie => cookie.name === 'foo1'));
          chrome.test.assertTrue(
              !!cookies.find(cookie => cookie.name === 'foo2'));
        }
        chrome.test.succeed();
      });
    });
  });
}

var tests = [
  function testCookieWithoutRules() {
    navigateTab(getServerURL('set-cookie?foo1=bar1&foo2=bar2'), function() {
      checkCookieHeaderRemoved(false);
    });
  },

  function addRulesAndTestCookieRemoval() {
    var rules = [{
      id: 1,
      condition: {urlFilter: host, resourceTypes: ['main_frame']},
      action: {type: 'removeHeaders', removeHeadersList: ['cookie']}
    }];
    chrome.declarativeNetRequest.addDynamicRules(rules, function() {
      chrome.test.assertNoLastError();
      checkCookieHeaderRemoved(true);
    });
  },

  function testSetCookieWithoutRules() {
    checkSetCookieHeaderRemoved(false);
  },

  function addRulesAndTestSetCookieRemoval() {
    var rules = [{
      id: 2,
      condition: {urlFilter: host, resourceTypes: ['main_frame']},
      action: {type: 'removeHeaders', removeHeadersList: ['setCookie']}
    }];
    chrome.declarativeNetRequest.addDynamicRules(rules, function() {
      chrome.test.assertNoLastError();
      checkSetCookieHeaderRemoved(true);
    });
  }
];

chrome.test.getConfig(function(config) {
  testServerPort = config.testServer.port;
  chrome.test.runTests(tests);
});
