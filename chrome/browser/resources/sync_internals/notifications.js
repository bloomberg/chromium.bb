// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

// TODO(akalin): Use table.js.

function updateNotificationsEnabledInfo(notificationsEnabled) {
  var notificationsEnabledInfo = $('notificationsEnabledInfo');
  jstProcess(
      new JsEvalContext({ 'notificationsEnabled': notificationsEnabled }),
      notificationsEnabledInfo);
}

// Contains all notification data.  The keys are sync types (as strings) and
// the value is a dictionary with:
//
//   type: the sync type again (for convenience when using JsTemplate)
//   totalCount: Number of notifications received since browser start.
//   sessionCount: Number of notifications received this
//                 chrome://sync-internals session.
//   payload: The last received payload.
//
chrome.sync.notifications = {};

/**
 * Merges d1 and d2 (with d2 taking precedence) and returns the result.
 */
function mergeDictionaries(d1, d2) {
  var d = {};
  for (var k in d1) {
    d[k] = d1[k];
  }
  for (var k in d2) {
    d[k] = d2[k];
  }
  return d;
}

/**
 * Merge notificationInfo into chrome.sync.notifications.
 */
function updateNotificationsFromNotificationInfo(notificationInfo) {
  for (var k in notificationInfo) {
    chrome.sync.notifications[k] =
        mergeDictionaries(chrome.sync.notifications[k] || {},
                          notificationInfo[k]);
    // notificationInfo[k] has values for the totalCount and payload keys,
    // so fill in the rest (if necessary).
    chrome.sync.notifications[k].type = k;
    chrome.sync.notifications[k].sessionCount =
        chrome.sync.notifications[k].sessionCount || 0;
  }
}

function incrementSessionNotificationCount(changedType) {
  chrome.sync.notifications[changedType].sessionCount =
      chrome.sync.notifications[changedType].sessionCount || 0;
  ++chrome.sync.notifications[changedType].sessionCount;
}

function updateNotificationInfoTable() {
  var notificationInfoTable = $('notificationInfo');
  var infos = [];
  for (var k in chrome.sync.notifications) {
    infos.push(chrome.sync.notifications[k]);
  }
  jstProcess(new JsEvalContext({ 'notifications': infos }),
             notificationInfoTable);
}

function updateNotificationInfo(notificationInfo) {
  updateNotificationsFromNotificationInfo(notificationInfo);
  updateNotificationInfoTable();
}

function onLoad() {
  chrome.sync.getNotificationState(updateNotificationsEnabledInfo);
  chrome.sync.getNotificationInfo(updateNotificationInfo);
  chrome.sync.onNotificationStateChange.addListener(
      function(details) { updateNotificationsEnabledInfo(details.enabled); });

  chrome.sync.onIncomingNotification.addListener(function(details) {
    var changedTypes = details.changedTypes;
    for (var i = 0; i < changedTypes.length; ++i) {
      incrementSessionNotificationCount(changedTypes[i]);
    }
    updateNotificationInfoTable();

    // Also update total counts.
    chrome.sync.getNotificationInfo(updateNotificationInfo);
  });
}

document.addEventListener('DOMContentLoaded', onLoad, false);
})();
