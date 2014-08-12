// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const notificationProvider = chrome.notificationProvider;

function createNotification(notificationId, options) {
  return new Promise(function (resolve, reject) {
    chrome.notifications.create(notificationId, options, function (id) {
      if (chrome.runtime.lastError) {
        reject(new Error("Unable to create notification"));
        return;
      }
      resolve(id);
      return;
    });
  });
};

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
  var myId = chrome.runtime.id;
  var id1 = "id1";
  var returnId = myId + "-" + id1;
  var content = {
    type: "basic",
    iconUrl: "icon.png",
    title: "Title",
    message: "This is the message."
  };

  // Create a notification, so there will be one existing notification
  createNotification(id1, content)
    .catch(function() { failTest("notifications.create"); })
    // Notify the sender that a notificaion was clicked.
    .then(function() { return notifyOnClicked(myId, returnId); })
    .catch(function() { failTest("NotifyOnClicked1"); })
    // Try to notify that an non-existent notification was clicked.
    .then(function() { return notifyOnClicked(myId, "doesNotExist"); })
    // Fail if it returns true.
    .then(function() { failTest("NotifyOnClicked2"); })
    // Notify the sender that a notificaion button was clicked.
    .catch(function() { return notifyOnButtonClicked(myId, returnId, 0); })
    .catch(function() { failTest("NotifyOnButtonClicked"); })
    // Try to notify that a non-existent notification button was clicked.
    .then(function() { return notifyOnButtonClicked(myId, "doesNotExist", 0)})
    .then(function() { failTest("NotifyOnButtonClicked"); })
    // Try to notify that an non-existent notification was cleared.
    .catch(function () { return notifyOnCleared(myId, "doesNotExist"); })
    .then(function() { failTest("NotifyOnCleared"); })
    // Notify that the original notification was cleared.
    .catch(function() { return notifyOnCleared(myId, returnId); })
    .catch(function() { failTest("NotifyOnCleared"); })
    .then(function () { return notifyOnPermissionLevelChanged(myId,
                                                              "granted"); })
    .catch(function() { failTest("NotifyOnPermissionLevelChanged"); })
    .then(function () { return notifyOnShowSettings(myId); })
    .catch(function() { failTest("NotifyOnShowSettings"); })
    .then(function() { chrome.test.succeed(); });
};

chrome.test.runTests([ testFunctions ]);
