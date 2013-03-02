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
var tryPollFrequencyInMs = 30000;

var prefs = new Prefs;

function updateBadgeOnErrorStatus() {
  chrome.browserAction.setBadgeText({text:"?"});
  chrome.browserAction.setBadgeBackgroundColor({color:[0,0,255,255]});
}

var lastNotification = null;
function notifyStatusChange(treeState, status) {
  if (lastNotification)
    lastNotification.cancel();

  var notification = webkitNotifications.createNotification(
    chrome.extension.getURL("icon.png"), "Tree is " + treeState, status);
  lastNotification = notification;
  notification.show();
}

// The type parameter should be "open", "closed", or "throttled".
function getLastStatusTime(callback, type) {
  requestURL(statusHistoryURL, "text", function(text) {
    var entries = JSON.parse(text);

    for (var i = 0; i < entries.length; i++) {
      if (entries[i].general_state == type) {
        callback(new Date(entries[i].date + " UTC"));
        return;
      }
    }
  }, updateBadgeOnErrorStatus);
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
    prefs.getUseNotifications(function(useNotifications) {
      if (useNotifications)
        notifyStatusChange(treeState, status);
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
  requestURL(statusURL, "text", updateStatus, updateBadgeOnErrorStatus);
  setTimeout(requestStatus, pollFrequencyInMs);
}

var activeIssues = {};
// Record of the last defunct build number we're aware of on each builder.  If
// the build number is less than or equal to this number, the buildbot
// information is not available and a request will return a 404.
var lastDefunctTryJob = {};

function fetchTryJobResults(fullPatchset, builder, buildnumber) {
  var tryJobURL =
    "http://build.chromium.org/p/tryserver.chromium/json/builders/" +
        builder + "/builds/" + buildnumber;

  if (lastDefunctTryJob.hasOwnProperty(builder) &&
      buildnumber <= lastDefunctTryJob[builder])
    return;

  var onStatusError = function(status) {
    if (status == 404)
      lastDefunctTryJob[builder] = buildnumber;
  };

  requestURL(tryJobURL, "json", function(tryJobResult) {
    if (!fullPatchset.full_try_job_results)
      fullPatchset.full_try_job_results = {};

    var key = builder + "-" + buildnumber;
    fullPatchset.full_try_job_results[key] = tryJobResult;
  }, onStatusError);
}

function fetchPatches(issue, completed) {
  var patchsetsRetrieved = 0;
  issue.patchsets.forEach(function(patchset) {
    var patchURL = "https://codereview.chromium.org/api/" + issue.issue +
        "/" + patchset;

    requestURL(patchURL, "json", function(patch) {
      if (!issue.full_patchsets)
        issue.full_patchsets = {};

      issue.full_patchsets[patch.patchset] = patch;

      patch.try_job_results.forEach(function(results) {
        if (results.buildnumber)
          fetchTryJobResults(patch, results.builder, results.buildnumber);
      });

      if (++patchsetsRetrieved == issue.patchsets.length)
        completed(issue);
    });
  });
}

function updateTryStatus(status) {
  var seen = {};
  status.results.forEach(function(result) {
    var issueURL = "https://codereview.chromium.org/api/" + result.issue;

    requestURL(issueURL, "json", function(issue) {
      fetchPatches(issue, function() {activeIssues[issue.issue] = issue;});
    });

    seen[result.issue] = true;
  });

  for (var issue in activeIssues)
    if (!seen[issue])
      delete activeIssues[issue];
}

function requestTryStatus() {
  prefs.getTryJobUsername(function(username) {
    if (username) {
      var url = "https://codereview.chromium.org/search" +
          // commit=2 is CLs with commit bit set, commit=3 is CLs with commit
          // bit cleared, commit=1 is either.
          "?closed=3&commit=1&limit=100&order=-modified&format=json&owner=" +
              username;
      requestURL(url, "json", updateTryStatus);
    }

    setTimeout(requestTryStatus, tryPollFrequencyInMs);
  });
}

function main() {
  requestStatus();
  requestTryStatus();
}

main();
