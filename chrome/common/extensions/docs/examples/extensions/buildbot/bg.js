// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(wittman): Convert this extension to event pages once they work with
// the notifications API.  Currently it's not possible to restore the
// Notification object when event pages get reloaded.  See
// http://crbug.com/165276.

var statusURL = "http://chromium-status.appspot.com/current?format=raw";
var statusHistoryURL =
  "http://chromium-status.appspot.com/allstatus?limit=20&format=json";
var pollFrequencyInMs = 30000;

function getUseNotifications(callback) {
  chrome.storage.sync.get(
    // Default to notifications being off.
    {prefs: {use_notifications: false}},
    function(storage) { callback(storage.prefs.use_notifications); });
}

var lastNotification = null;
function notifyStatusChange(treeState, status) {
  if (lastNotification) {
    lastNotification.cancel();
  }
  var notification = webkitNotifications.createNotification(
    chrome.extension.getURL("icon.png"), "Tree is " + treeState, status);
  lastNotification = notification;
  notification.show();
}

// The type parameter should be "open", "closed", or "throttled".
function getLastStatusTime(callback, type) {
  requestURL(statusHistoryURL, function(text) {
    var entries = JSON.parse(text);

    for (var i = 0; i < entries.length; i++) {
      if (entries[i].general_state == type) {
        callback(new Date(entries[i].date + " UTC"));
        return;
      }
    }
  }, "text");
}

function updateTimeBadge(timeDeltaInMs) {
  var secondsSinceChangeEvent = Math.round(timeDeltaInMs / 1000);
  var minutesSinceChangeEvent = Math.round(secondsSinceChangeEvent / 60);
  var hoursSinceChangeEvent = Math.round(minutesSinceChangeEvent / 60);
  var daysSinceChangeEvent = Math.round(hoursSinceChangeEvent / 24);

  var text;
  if (secondsSinceChangeEvent < 60) {
    text = "<1m";
  } else if (minutesSinceChangeEvent < 57.5) {
    if (minutesSinceChangeEvent < 30) {
      text = minutesSinceChangeEvent + "m";
    } else {
      text = Math.round(minutesSinceChangeEvent / 5) * 5 + "m";
    }
  } else if (minutesSinceChangeEvent < 5 * 60) {
      var halfHours = Math.round(minutesSinceChangeEvent / 30);
      text = Math.floor(halfHours / 2) + (halfHours % 2 ? ".5" : "") + "h";
  } else if (hoursSinceChangeEvent < 23.5) {
    text = hoursSinceChangeEvent + "h";
  } else {
    text = daysSinceChangeEvent + "d";
  }

  chrome.browserAction.setBadgeText({text: text});
}

var lastState;
var lastChangeTime;
function updateStatus(status) {
  var badgeState = {
    open: {color: [0,255,0,255], defaultText: "\u2022"},
    closed: {color: [255,0,0,255], defaultText: "\u00D7"},
    throttled: {color: [255,255,0,255], defaultText: "!"}
  };

  chrome.browserAction.setTitle({title:status});
  var treeState = (/open/i).exec(status) ? "open" :
      (/throttled/i).exec(status) ? "throttled" : "closed";

  if (lastState && lastState != treeState) {
    getUseNotifications(function(useNotifications) {
      if (useNotifications) {
        notifyStatusChange(treeState, status);
      }
    });
  }

  chrome.browserAction.setBadgeBackgroundColor(
      {color: badgeState[treeState].color});

  if (lastChangeTime === undefined) {
    chrome.browserAction.setBadgeText(
        {text: badgeState[treeState].defaultText});
    lastState = treeState;
    getLastStatusTime(function(time) {
      lastChangeTime = time;
      updateTimeBadge(Date.now() - lastChangeTime);
    }, treeState);
  } else {
    if (treeState != lastState) {
      lastState = treeState;
      // The change event will occur 1/2 the polling frequency before we
      // are aware of it, on average.
      lastChangeTime = Date.now() - pollFrequencyInMs / 2;
    }
    updateTimeBadge(Date.now() - lastChangeTime);
  }
}

function requestStatus() {
  requestURL(statusURL, updateStatus, "text");
  setTimeout(requestStatus, pollFrequencyInMs);
}

function requestURL(url, callback, opt_responseType) {
  var xhr = new XMLHttpRequest();
  xhr.responseType = opt_responseType || "document";

  xhr.onreadystatechange = function(state) {
    if (xhr.readyState == 4) {
      if (xhr.status == 200) {
        callback(xhr.responseType == "document" ?
                 xhr.responseXML : xhr.responseText);
      } else {
        chrome.browserAction.setBadgeText({text:"?"});
        chrome.browserAction.setBadgeBackgroundColor({color:[0,0,255,255]});
      }
    }
  };

  xhr.onerror = function(error) {
    console.log("xhr error:", error);
  };

  xhr.open("GET", url, true);
  xhr.send();
}

function main() {
  requestStatus();
}

main();
