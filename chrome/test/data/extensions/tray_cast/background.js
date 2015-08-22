// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var backgroundSetup = {}

// If all required activity information is available, then this will return an
// activity structure; otherwise, it will return null. This lets us
// unconditionally assign to the 'activity' field in a JSON object.
tryCreateActivity_ = function(id, title, tabId) {
  if (id === undefined || title === undefined || tabId === undefined)
    return null;

  return {
    'id': id,
    'title': title,
    'tabId': tabId
  };
}

var receiversActivities = [];
var sendDevices = function() {
  chrome.cast.devicesPrivate.updateDevices(receiversActivities);
}
chrome.cast.devicesPrivate.updateDevicesRequested.addListener(sendDevices);

// Add a new receiver. |activityTitle| and |activityTabId| are optional
// parameters.
var addReceiver = function(id, receiverName, activityTitle, activityTabId) {
  receiversActivities.push({
    'receiver': {
      'id': id,
      'name': receiverName
    },
    'activity': tryCreateActivity_(id, activityTitle, activityTabId)
  });

  sendDevices();
}

var stopMirroringCalled = false;
chrome.cast.devicesPrivate.stopCast.addListener(function(reason) {
  if (reason !== 'user-stop')
    throw 'expected reason to be "user-stop"';

  var foundActivity = false;
  for (item of receiversActivities) {
    if (item.activity != null) {
      item.activity = null;
      foundActivity = true;
    }
  }
  if (foundActivity === false)
    throw 'stopMirroring called when there was nothing being mirrored'

  stopMirroringCalled = true;
  sendDevices();
});


var launchDesktopMirroringReceiverId = '';
chrome.cast.devicesPrivate.startCast.addListener(function(receiverId) {
  launchDesktopMirroringReceiverId = receiverId;

  var tabTitle = 'Tab Title';
  var tabId = 1;

  for (item of receiversActivities) {
    if (item.receiver.id == receiverId) {
      item.activity = tryCreateActivity_(receiverId, tabTitle, tabId);
      break;
    }
  }

  sendDevices();
});
