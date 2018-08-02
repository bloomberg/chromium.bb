// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var dataURL = 'data:text/plain,redirected1';

function getURLNonWebAccessible() {
  return getURL('manifest.json');
}

function getURLWebAccessible() {
  return getURL('simpleLoad/a.html');
}

function assertRedirectSucceeds(url, redirectURL, callback) {
  navigateAndWait(url, function(tab) {
    callback && callback();
    chrome.test.assertEq(redirectURL, tab.url);
  });
}

function assertRedirectFails(url) {
  navigateAndWait(url, function(tab) {
    // Tab URL should still be set to the original URL.
    chrome.test.assertEq(url, tab.url);
  });
}

runTests([
  function redirectToDataUrlOnHeadersReceived() {
    var url = getServerURL('echo');
    var listener = function(details) {
      return {redirectUrl: dataURL};
    };
    chrome.webRequest.onHeadersReceived.addListener(listener,
        {urls: [url]}, ['blocking']);

    assertRedirectSucceeds(url, dataURL, function() {
      chrome.webRequest.onHeadersReceived.removeListener(listener);
    });
  },

  function redirectToNonWebAccessibleUrlOnHeadersReceived() {
    var url = getServerURL('echo');
    var listener = function(details) {
      return {redirectUrl: getURLNonWebAccessible()};
    };
    chrome.webRequest.onHeadersReceived.addListener(listener,
        {urls: [url]}, ['blocking']);

    assertRedirectSucceeds(url, getURLNonWebAccessible(), function() {
      chrome.webRequest.onHeadersReceived.removeListener(listener);
    });
  },

  function redirectToServerRedirectOnHeadersReceived() {
    var url = getServerURL('echo');
    var redirectURL = getServerURL('server-redirect?' + getURLWebAccessible());
    var listener = function(details) {
      return {redirectUrl: redirectURL};
    };
    chrome.webRequest.onHeadersReceived.addListener(listener,
        {urls: [url]}, ['blocking']);

    assertRedirectSucceeds(url, getURLWebAccessible(), function() {
      chrome.webRequest.onHeadersReceived.removeListener(listener);
    });
  },

  function redirectToDataUrlOnBeforeRequest() {
    var url = getServerURL('echo');
    var listener = function(details) {
      return {redirectUrl: dataURL};
    };
    chrome.webRequest.onBeforeRequest.addListener(listener,
        {urls: [url]}, ['blocking']);

    assertRedirectSucceeds(url, dataURL, function() {
      chrome.webRequest.onBeforeRequest.removeListener(listener);
    });
  },

  function redirectToNonWebAccessibleUrlOnBeforeRequest() {
    var url = getServerURL('echo');
    var listener = function(details) {
      return {redirectUrl: getURLNonWebAccessible()};
    };
    chrome.webRequest.onBeforeRequest.addListener(listener,
        {urls: [url]}, ['blocking']);

    assertRedirectSucceeds(url, getURLNonWebAccessible(), function() {
      chrome.webRequest.onBeforeRequest.removeListener(listener);
    });
  },

  function redirectToServerRedirectOnBeforeRequest() {
    var url = getServerURL('echo');
    var redirectURL = getServerURL('server-redirect?' + getURLWebAccessible());
    var listener = function(details) {
      return {redirectUrl: redirectURL};
    };
    chrome.webRequest.onBeforeRequest.addListener(listener,
        {urls: [url]}, ['blocking']);

    assertRedirectSucceeds(url, getURLWebAccessible(), function() {
      chrome.webRequest.onBeforeRequest.removeListener(listener);
    });
  },

  function redirectToDataUrlWithServerRedirect() {
    assertRedirectFails(getServerURL('server-redirect?' + dataURL));
  },

  function redirectToNonWebAccessibleUrlWithServerRedirect() {
    assertRedirectFails(
        getServerURL('server-redirect?' + getURLNonWebAccessible()));
  },

  function redirectToWebAccessibleUrlWithServerRedirect() {
    assertRedirectSucceeds(
        getServerURL('server-redirect?' + getURLWebAccessible()),
        getURLWebAccessible());
  },
]);
