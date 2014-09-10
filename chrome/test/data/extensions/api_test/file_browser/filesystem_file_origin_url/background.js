// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Reports the promise state to the test system.
 *
 * @param {Promise} promise Promise to be resolved with the test result.
 */
function reportPromise(promise) {
  promise.then(
      chrome.test.callbackPass(),
      function(error) {
        chrome.test.fail(error.stack || error);
      });
}

/**
 * Checks if the promise is rejected or not.
 * If the promise is fulfilled, it causes test failure.
 *
 * @param {Promise} promise Promise expected to reject.
 * @return {Promise} Promise that is fulfilled when the given promise is
 *     rejected.
 */
function expectRejection(promise) {
  var errorMessage = new Error('Promise is fulfilled unintentionally.').stack;
  return promise.then(function() {
    chrome.test.fail(errorMessage);
    return Promise.reject(errorMessage);
  }, function(arg) {
    /* Recover rejection. */
    return arg;
  });
}

/**
 * Calls webkitResolveLocalFileSystemURL, and returns the result promise.
 *
 * @param {string} url URL.
 * @return {Promise} Promise to be fulfilled/rejected with the result.
 */
function resolveLocalFileSystemURL(url) {
  return new Promise(webkitResolveLocalFileSystemURL.bind(null, url));
}

/**
 * Sends XHR, and returns the result promise.
 *
 * @param {string} url URL.
 * @return {Promise} Promise to be fulfilled/rejected with the result.
 */
function sendXHR(url) {
  return new Promise(function(fulfill, reject) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url);
    xhr.onload = fulfill;
    try {
      // XHR.send sends DOMException for security error.
      xhr.send();
    } catch (exception) {
      reject('SECURITY_ERROR');
    }
    xhr.onerror = function() {
      reject('ERROR_STATUS: ' + xhr.status);
    };
  });
}

/**
 * Requests the drive file system.
 *
 * @return {Promise} Promise to be fulfilled with the drive file system.
 */
function requestDriveFileSystem() {
  return new Promise(function(fulfill) {
    chrome.fileManagerPrivate.requestFileSystem(
        'drive:drive-user',
        function(fileSystem) {
          chrome.test.assertTrue(!!fileSystem);
          fulfill(fileSystem);
        });
  });
}

/**
 * Ensures resolveLocalFileSystemURL does not resolve filesystem:file/// URL.
 *
 * @return {Promise} Promise to be resolved with the test result.
 */
function testResolveFileSystemURL() {
  reportPromise(requestDriveFileSystem().then(function(fileSystem) {
    return Promise.all([
        resolveLocalFileSystemURL(
            'filesystem:chrome-extension://kidcpjlbjdmcnmccjhjdckhbngnhnepk/' +
            'external/drive/root/test_dir/test_file.xul'),
        expectRejection(resolveLocalFileSystemURL(
            'filesystem:file:///external/drive/root/test_dir/test_file.xul'))
    ]);
  }));
}

/**
 * Ensures filesystem:file/// URL does not respond to XHR.
 *
 * @return {Promise} Promise to be resolved with the test result.
 */
function testSendXHRToFileSystemURL() {
  // Thus we grant the permission to the extension in
  // chrome.fileManagerPrivate.requestFileSystem, we need to call the method
  // before.
  reportPromise(requestDriveFileSystem().then(function(fileSystem) {
    return Promise.all([
        sendXHR(
            'filesystem:chrome-extension://kidcpjlbjdmcnmccjhjdckhbngnhnepk/' +
            'external/drive/root/test_dir/test_file.xul'),
        expectRejection(sendXHR(
            'filesystem:file:///external/drive/root/test_dir/test_file.xul')).
            then(function(error) {
              chrome.test.assertEq('ERROR_STATUS: 0', error);
            })
    ]);
  }));
}

// Run tests.
chrome.test.runTests([
  testResolveFileSystemURL,
  testSendXHRToFileSystemURL
]);
