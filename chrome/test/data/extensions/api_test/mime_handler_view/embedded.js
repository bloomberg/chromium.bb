// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

window.addEventListener('message', function(event) {
  // Echo the data back to the source window.
  event.source.postMessage(event.data, '*');
}, false);

var streamDetails;

function fetchUrl(url) {
  return new Promise(function(resolve, reject) {
    var request = new XMLHttpRequest();
    request.onreadystatechange = function() {
      if (request.readyState == 4) {
        resolve({
          status: request.status,
          data: request.responseText,
        });
      }
    };
    request.open('GET', streamDetails.streamUrl, true);
    request.send();
  });
}

var tests = [
  function testBasic() {
    chrome.test.assertEq(
        'chrome-extension://oickdpebdnfbgkcaoklfcdhjniefkcji/testBasic.csv',
        streamDetails.originalUrl);
    chrome.test.assertEq('text/csv', streamDetails.mimeType);
    chrome.test.assertTrue(streamDetails.tabId != -1);
    chrome.test.assertFalse(streamDetails.embedded);

    fetchUrl(streamDetails.streamUrl).then(function(response) {
      chrome.test.assertEq(200, response.status);
      chrome.test.assertEq('content to read\n', response.data);
      chrome.test.succeed();
    });
  },

  function testEmbedded() {
    chrome.test.assertEq(
        'chrome-extension://oickdpebdnfbgkcaoklfcdhjniefkcji/testEmbedded.csv',
        streamDetails.originalUrl);
    chrome.test.assertEq('text/csv', streamDetails.mimeType);
    chrome.test.assertTrue(streamDetails.tabId != -1);
    chrome.test.assertTrue(streamDetails.embedded);

    fetchUrl(streamDetails.streamUrl).then(function(response) {
      chrome.test.assertEq(200, response.status);
      chrome.test.assertEq('content to read\n', response.data);
      chrome.test.succeed();
    });
  },

  function testAbort() {
    chrome.mimeHandlerPrivate.abortStream(function() {
      fetchUrl(streamDetails.streamUrl).then(function(response) {
        chrome.test.assertEq(404, response.status);
        chrome.test.assertEq('', response.data);
        chrome.test.succeed();
      });
    });
  },
];

var testsByName = {};
for (let i = 0; i < tests.length; i++) {
  testsByName[tests[i].name] = tests[i];
}

chrome.mimeHandlerPrivate.getStreamInfo(function(streamInfo) {
  if (!streamInfo)
    return;

  // If the name of the file we're handling matches the name of a test, run that
  // test.
  var urlComponents = streamInfo.originalUrl.split('/');
  var test = urlComponents[urlComponents.length - 1].split('.')[0];
  streamDetails = streamInfo;
  if (testsByName[test])
    chrome.test.runTests([testsByName[test]]);
});
