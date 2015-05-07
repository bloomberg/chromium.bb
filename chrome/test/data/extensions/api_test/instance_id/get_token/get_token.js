// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getTokenWithoutParameters() {
  try {
    chrome.instanceID.getToken();
    chrome.test.fail(
        "Calling getToken without parameters should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function getTokenWithoutCallback() {
  try {
    chrome.instanceID.getToken({"authorizedEntity": "1", "scope": "GCM"});
    chrome.test.fail(
        "Calling getToken without callback should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function getTokenWithoutAuthorizedEntity() {
  try {
    chrome.instanceID.getToken({"scope": "GCM"});
    chrome.test.fail(
        "Calling getToken without authorizedEntity parameter should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function getTokenWithInvalidAuthorizedEntity() {
  try {
    chrome.instanceID.getToken({"authorizedEntity": 1, "scope": "GCM"});
    chrome.test.fail(
        "Calling getToken with invalid authorizedEntity parameter " +
        "should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function getTokenWithoutScope() {
  try {
    chrome.instanceID.getToken({"authorizedEntity": "1"});
    chrome.test.fail(
        "Calling getToken without scope parameter should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function getTokenWithInvalidScope() {
  try {
    chrome.instanceID.getToken({"authorizedEntity": "1", "scope": 1});
    chrome.test.fail(
        "Calling getToken with invalid scope parameter should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function getTokenWithInvalidOptionValue() {
  try {
    chrome.instanceID.getToken(
      {"authorizedEntity": "1", "scope": "GCM", "options": {"foo": 1}},
      function(token) {
        if (chrome.runtime.lastError) {
          chrome.test.succeed();
          return;
        }
        chrome.test.fail(
            "Calling getToken with invalid option value should fail.");
      }
    );
  } catch (e) {
    chrome.test.succeed();
  };
}

function getTokenWithoutOptions() {
  chrome.instanceID.getToken(
    {"authorizedEntity": "1", "scope": "GCM"},
    function(token) {
      if (chrome.runtime.lastError) {
        chrome.test.fail(
            "chrome.runtime.lastError: " + chrome.runtime.lastError.message);
        return;
      }
      if (!token) {
        chrome.test.fail("Empty token returned.");
        return;
      }

      chrome.test.succeed();
    }
  );
}

function getTokenWithValidOptions() {
  chrome.instanceID.getToken(
    {"authorizedEntity": "1", "scope": "GCM", "options": {"foo": "1"}},
    function(token) {
      if (chrome.runtime.lastError) {
        chrome.test.fail(
            "chrome.runtime.lastError: " + chrome.runtime.lastError.message);
        return;
      }
      if (!token) {
        chrome.test.fail("Empty token returned.");
        return;
      }

      chrome.test.succeed();
    }
  );
}

chrome.test.runTests([
  getTokenWithoutParameters,
  getTokenWithoutCallback,
  getTokenWithoutAuthorizedEntity,
  getTokenWithInvalidAuthorizedEntity,
  getTokenWithoutScope,
  getTokenWithInvalidScope,
  getTokenWithInvalidOptionValue,
  // TODO(jianli): To be enabled when GetToken is implemented.
  //getTokenWithoutOptions,
  //getTokenWithValidOptions,
]);
