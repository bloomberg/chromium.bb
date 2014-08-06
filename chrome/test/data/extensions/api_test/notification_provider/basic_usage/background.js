// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const notificationProvider = chrome.notificationProvider;

function notifyOnCleared(senderId, notificationId) {
  return new Promise(function (resolve, reject) {
    notificationProvider.notifyOnCleared(senderId,
                                         notificationId,
                                         function (wasCleared) {
      if (chrome.runtime.lastError || !wasCleared) {
        reject(new Error("Unable to send onClear message"));
        return;
      }
      resolve(wasCleared);
      return;
    });
  });
};

function notifyOnClicked(senderId, notificationId) {
  return new Promise(function (resolve, reject) {
    notificationProvider.notifyOnClicked(senderId,
                                         notificationId,
                                         function (matchExists) {
      if (chrome.runtime.lastError || !matchExists) {
        reject(new Error("Unable to send onClick message"));
        return;
      }
      resolve(matchExists);
      return;
    });
  });
};

function notifyOnButtonClicked(senderId, notificationId, buttonIndex) {
  return new Promise(function (resolve, reject) {
    notificationProvider.notifyOnButtonClicked(senderId,
                                               notificationId,
                                               buttonIndex,
                                               function (matchExists) {
      if (chrome.runtime.lastError || !matchExists) {
        reject(new Error("Unable to send onButtonClick message"));
        return;
      }
      resolve(matchExists);
      return;
    });
  });
};

function notifyOnPermissionLevelChanged(senderId, permissionLevel) {
  return new Promise(function (resolve, reject) {
    notificationProvider.notifyOnPermissionLevelChanged(
                                              senderId,
                                              permissionLevel,
                                              function (notifierExists) {
      if (chrome.runtime.lastError || !notifierExists) {
        reject(new Error("Unable to send onPermissionLevelChanged message"));
        return;
      }
      resolve(notifierExists);
      return;
    });
  });
};

function notifyOnShowSettings(senderId) {
  return new Promise(function (resolve, reject) {
    notificationProvider.notifyOnShowSettings(senderId,
                                              function (notifierExists) {
      if (chrome.runtime.lastError || !notifierExists) {
        reject(new Error("Unable to send onShowSettings message"));
        return;
      }
      resolve(notifierExists);
      return;
    });
  });
};

function failTest(testName) {
  chrome.test.fail(testName);
};

function testFunctions() {
  var testName = "testFunctions";
  var notifierId;
  var notId;
  notificationProvider.onCreated.addListener(function(senderId,
                                                      notificationId,
                                                      content) {
    notifierId = senderId;
    notId = notificationId;

    notifyOnClicked(notifierId, notId)
      .catch(function() { failTest("NotifyOnClickedFailed"); })
      .then(function() { return notifyOnButtonClicked(notifierId, notId, 0); })
      .catch(function() { failTest("NotifyOnButtonClickedFailed"); })
      .then(function () { return notifyOnCleared(notifierId, notId); })
      .catch(function() { failTest("NotifyOnClearedFailed"); })
      .then(function () { return notifyOnPermissionLevelChanged(notifierId,
                                                                "granted"); })
      .catch(function() { failTest("NotifyOnPermissionLevelChangedFailed"); })
      .then(function () { return notifyOnShowSettings(notifierId); })
      .catch(function() { failTest("NotifyOnShowSettingsFailed"); })
      .then(function() { chrome.test.succeed(testName); });
  });
};

chrome.test.runTests([ testFunctions ]);
