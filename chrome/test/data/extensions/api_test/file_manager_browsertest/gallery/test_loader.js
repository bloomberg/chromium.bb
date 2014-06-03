// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Loads a test script from URL.
 * @param {string} url URL of a script file.
 * @return {Promise} Promise to be fulfilled when completing to load.
 */
function loadScript(url) {
  return new Promise(function(fulfill, reject) {
    var script = document.createElement('script');
    script.src = url;
    // Ensure the scripts are loaded in the order of calling loadScript.
    script.async = false;
    script.onload = fulfill;
    script.onerror = reject.bind(null, 'Faile to load ' + url);
    document.documentElement.appendChild(script);
  });
}

var testUtilPromise = loadScript(
    'chrome-extension:///ejhcmmdhhpdhhgmifplfmjobgegbibkn/test_util.js');

testUtilPromise.then(function() {
  var inGuestModePromise = sendTestMessage({name: 'isInGuestMode'});
  var testNamePromise = sendTestMessage({name: 'getTestName'});
  var scriptsPromise = sendTestMessage({name: 'getScripts'});
  Promise.all([inGuestModePromise, testNamePromise, scriptsPromise]).then(
      function(args) {
        // Do nothing if the guest mode is different.
        if (JSON.parse(args[0]) !== chrome.extension.inIncognitoContext)
          return;
        var scripts = JSON.parse(args[2]);
        return Promise.all(scripts.map(loadScript)).then(function() {
          var testName = args[1];
          var testCase = function() {
            var success = chrome.test.callbackPass();
            Promise.resolve().then(function() {
              return window[testName]();
            }).then(function() {
              success();
            }, function(error) {
              chrome.test.fail(error.stack || error);
            });
          };
          testCase.generatedName = testName;
          chrome.test.runTests([testCase]);
        });
      });
}).catch(function(error) {
  chrome.test.fail(error.stack || error);
});
