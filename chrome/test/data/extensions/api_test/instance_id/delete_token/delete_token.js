// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function dummyGetTokenCompleted(token) {
}

function deleteTokenWithoutParameters() {
  try {
    chrome.instanceID.deleteToken();
    chrome.test.fail(
        "Calling deleteToken without parameters should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteTokenWithoutCallback() {
  try {
    chrome.instanceID.deleteToken({"authorizedEntity": "1", "scope": "GCM"});
    chrome.test.fail(
        "Calling deleteToken without callback should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteTokenWithoutAuthorizedEntity() {
  try {
    chrome.instanceID.deleteToken({"scope": "GCM"}, dummyGetTokenCompleted);
    chrome.test.fail(
        "Calling deleteToken without authorizedEntity parameter should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteTokenWithInvalidAuthorizedEntity() {
  try {
    chrome.instanceID.deleteToken(
        {"authorizedEntity": 1, "scope": "GCM"},
        dummyGetTokenCompleted);
    chrome.test.fail(
        "Calling deleteToken with invalid authorizedEntity parameter " +
        "should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteTokenWithoutScope() {
  try {
    chrome.instanceID.deleteToken(
        {"authorizedEntity": "1"}, dummyGetTokenCompleted);
    chrome.test.fail(
        "Calling deleteToken without scope parameter should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteTokenWithInvalidScope() {
  try {
    chrome.instanceID.deleteToken(
        {"authorizedEntity": "1", "scope": 1}, dummyGetTokenCompleted);
    chrome.test.fail(
        "Calling deleteToken with invalid scope parameter should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteTokenBeforeGetToken() {
  chrome.instanceID.deleteToken(
    {"authorizedEntity": "1", "scope": "GCM"},
    function() {
      if (chrome.runtime.lastError) {
        chrome.test.fail(
            "chrome.runtime.lastError: " + chrome.runtime.lastError.message);
        return;
      }

      chrome.test.succeed();
    }
  );
}

function deleteTokenAfterGetToken() {
  chrome.instanceID.getToken(
    {"authorizedEntity": "1", "scope": "GCM"},
    function(token) {
      if (chrome.runtime.lastError || !token) {
        chrome.test.fail(
            "chrome.runtime.lastError was set or token was empty.");
        return;
      }
      chrome.instanceID.deleteToken(
        {"authorizedEntity": "1", "scope": "GCM"},
        function() {
          if (chrome.runtime.lastError) {
            chrome.test.fail("chrome.runtime.lastError: " +
                chrome.runtime.lastError.message);
            return;
          }

          chrome.test.succeed();
        }
      );
    }
  );
}

chrome.test.runTests([
  deleteTokenWithoutParameters,
  deleteTokenWithoutCallback,
  deleteTokenWithoutAuthorizedEntity,
  deleteTokenWithInvalidAuthorizedEntity,
  deleteTokenWithoutScope,
  deleteTokenWithInvalidScope,
  // TODO(jianli): To be enabled when deleteToken is implemented.
  //deleteTokenBeforeGetToken,
  //deleteTokenAfterGetToken,
  //getTokenDeleteTokeAndGetToken,
]);
