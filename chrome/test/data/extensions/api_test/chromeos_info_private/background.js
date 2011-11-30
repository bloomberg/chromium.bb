// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;

function getTestFunctionFor(keys, fails) {
  return function generatedTest () {
    // Debug.
    console.log("keys: " + keys + "; fails: " + fails);

    chrome.chromeosInfoPrivate.get(
        keys,
        pass(
          function(values) {
            for (var i = 0; i < keys.length; ++i) {
              // Debug
              if (keys[i] in values) {
                console.log("  values['" + keys[i] + "'] = " +
                            values[keys[i]]);
              } else {
                console.log("  " + keys[i] + " is missing in values");
              }

              chrome.test.assertEq(fails.indexOf(keys[i]) == -1,
                                   keys[i] in values);
            }
          }
        )
    );
  }
}

// Automatically generates tests for the given possible keys. Note, this
// tests do not check return value, only the fact that it is presented.
function generateTestsForKeys(keys) {
  var tests = [];
  // Test with all the keys at one.
  tests.push(getTestFunctionFor(keys, []));
  // Tests with key which hasn't corresponding value.
  var noValueKey = "noValueForThisKey";
  tests.push(getTestFunctionFor([noValueKey], [noValueKey]));

  if (keys.length > 1) {
    // Tests with the separate keys.
    for (var i = 0; i < keys.length; ++i) {
      tests.push(getTestFunctionFor([keys[i]], []));
    }
  }
  if (keys.length >= 2) {
    tests.push(getTestFunctionFor([keys[0], keys[1]], []));
    tests.push(getTestFunctionFor([keys[0], noValueKey, keys[1]],
                                  [noValueKey]));
  }
  return tests;
}

var tests = generateTestsForKeys(["hwid", "homeProvider", "initialLocale"])
chrome.test.runTests(tests);
