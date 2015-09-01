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
    "id": id,
    "title": title,
    "tabId": tabId
  };
}

var receiversActivities = [];
// Add a new receiver. |activityTitle| and |activityTabId| are optional
// parameters.
addReceiver = function(id, receiverName, activityTitle, activityTabId) {
  receiversActivities.push({
    "activity": tryCreateActivity_(id, activityTitle, activityTabId),
    "receiver": {
      "id": id,
      "name": receiverName
    }
  });
}
// Required API method.
getMirrorCapableReceiversAndActivities = function() {
  // For all of the API methods, we verify that |this| points to
  // backgroundSetup. In the actual extension, the API methods are
  // also free-standing but they are really class methods on backgroundSetup.
  if (this !== backgroundSetup)
    throw 'this !== backgroundSetup';

  return receiversActivities;
}

var stopMirroringReason = "";
var stopMirroringCalled = false;
wasStopMirroringCalledWithUserStop = function() {
  return stopMirroringCalled && stopMirroringReason == 'user-stop';
}
// Required API method.
stopMirroring = function(reason) {
  if (this !== backgroundSetup)
    throw 'this !== backgroundSetup';

  stopMirroringReason = reason;
  stopMirroringCalled = true;

  var foundActivity = false;
  for (item of receiversActivities) {
    if (item.activity != null) {
      item.activity = null;
      foundActivity = true;
    }
  }
  if (foundActivity === false)
    throw 'stopMirroring called when there was nothing being mirrored'
}

var launchTabId = 1;
var launchTabTitle = "Fake Cast";
var launchDesktopMirroringReceiverId = "";
getLaunchDesktopMirroringReceiverId = function() {
  return launchDesktopMirroringReceiverId;
}
// Required API method.
launchDesktopMirroring = function(receiverId) {
  if (this !== backgroundSetup)
    throw 'this !== backgroundSetup';

  launchDesktopMirroringReceiverId = receiverId;

  for (item of receiversActivities) {
    if (item.receiver.id == receiverId) {
      item.activity =
        tryCreateActivity_(receiverId, launchTabId, launchTabTitle);
      break;
    }
  }
}
