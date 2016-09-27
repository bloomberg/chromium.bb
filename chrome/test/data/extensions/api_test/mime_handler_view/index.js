// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Tests don't start running until an async call to
// chrome.mimeHandlerPrivate.getStreamInfo() completes, so queue any messages
// received until that point.
var queuedMessages = [];

function queueMessage(event) {
  queuedMessages.push(event);
}

window.addEventListener('message', queueMessage, false);

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

function expectSuccessfulRead(response) {
  chrome.test.assertEq(200, response.status);
  chrome.test.assertEq('content to read\n', response.data);
}

function expectSuccessfulReadLong(response) {
  chrome.test.assertEq(200, response.status);
  chrome.test.assertTrue(response.data.startsWith('content to read\n'));
}

function checkStreamDetails(name, embedded) {
  checkStreamDetailsNoFile();
  chrome.test.assertEq(embedded, streamDetails.embedded);
  chrome.test.assertTrue(streamDetails.originalUrl.indexOf(name) != -1);
  chrome.test.assertEq('text/csv',
                       streamDetails.responseHeaders['Content-Type']);
}

function checkStreamDetailsNoFile() {
  chrome.test.assertEq('text/csv', streamDetails.mimeType);
  chrome.test.assertTrue(streamDetails.tabId != -1);
}

var tests = [
  function testBasic() {
    checkStreamDetails('testBasic.csv', false);
    fetchUrl(streamDetails.streamUrl)
        .then(expectSuccessfulRead)
        .then(chrome.test.succeed);
  },

  function testEmbedded() {
    checkStreamDetails('testEmbedded.csv', true);
    fetchUrl(streamDetails.streamUrl)
        .then(expectSuccessfulRead)
        .then(chrome.test.succeed);
  },

  function testIframe() {
    checkStreamDetails('testIframe.csv', true);
    var printMessageArrived = new Promise(function(resolve, reject) {
      window.addEventListener('message', function(event) {
        chrome.test.assertEq('print', event.data.type);
        resolve();
      }, false);
    });
    var contentRead = fetchUrl(streamDetails.streamUrl)
        .then(expectSuccessfulRead);
    Promise.all([printMessageArrived, contentRead]).then(chrome.test.succeed);
  },

  function testAbort() {
    checkStreamDetails('testAbort.csv', false);
    chrome.mimeHandlerPrivate.abortStream(function() {
      fetchUrl(streamDetails.streamUrl).then(function(response) {
        chrome.test.assertEq(404, response.status);
        chrome.test.assertEq('', response.data);
        chrome.test.succeed();
      });
    });
  },

  function testNonAsciiHeaders() {
    checkStreamDetails('testNonAsciiHeaders.csv', false);
    chrome.test.assertEq(undefined,
                         streamDetails.responseHeaders['Content-Disposition']);
    chrome.test.assertEq(undefined,
                         streamDetails.responseHeaders['ü']);
    chrome.test.succeed();
  },

  function testPostMessage() {
    var expectedMessages = ['hey', 100, 25.0];
    var messagesReceived = 0;
    function handleMessage(event) {
      if (event.data == 'succeed' &&
          messagesReceived == expectedMessages.length) {
        chrome.test.succeed();
      } else if (event.data == 'fail') {
        chrome.test.fail();
      } else if (event.data == expectedMessages[messagesReceived]) {
        event.source.postMessage(event.data, '*');
        messagesReceived++;
      } else {
        chrome.test.fail('unexpected message ' + event.data);
      }
    }
    window.addEventListener('message', handleMessage, false);
    while (queuedMessages.length) {
      handleMessage(queuedMessages.shift());
    }
  },

  function testDataUrl() {
    // TODO(raymes): have separate checks for embedded/unembedded data URLs.
    checkStreamDetailsNoFile();
    fetchUrl(streamDetails.streamUrl)
        .then(expectSuccessfulRead)
        .then(chrome.test.succeed);
  },

  function testDataUrlLong() {
    checkStreamDetailsNoFile();
    fetchUrl(streamDetails.streamUrl)
        .then(expectSuccessfulReadLong)
        .then(chrome.test.succeed);
  },

  function testResizeBeforeAttach() {
    checkStreamDetails('testResizeBeforeAttach.csv', true);
    chrome.test.succeed();
  }
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
  if (testsByName[test]) {
    window.removeEventListener('message', queueMessage);
    chrome.test.runTests([testsByName[test]]);
  }

  // Run the test for data URLs.
  if (streamInfo.originalUrl.startsWith("data:")) {
    window.removeEventListener('message', queueMessage);
    // Long data URLs get truncated.
    if (streamInfo.originalUrl == "data:")
      chrome.test.runTests([testsByName['testDataUrlLong']]);
    else
      chrome.test.runTests([testsByName['testDataUrl']]);
  }
});
