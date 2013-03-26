// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview The event page for Google Now for Chrome implementation.
 * The Google Now event page gets Google Now cards from the server and shows
 * them as Chrome notifications.
 * The service performs periodic updating of Google Now cards.
 * Each updating of the cards includes 4 steps:
 * 1. Processing requests for cards dismissals that are not yet sent to the
 *    server;
 * 2. Obtaining the location of the machine;
 * 3. Making a server request based on that location;
 * 4. Showing the received cards as notifications.
 */

// TODO(vadimt): Use background permission to show notifications even when all
// browser windows are closed.
// TODO(vadimt): Remove the C++ implementation.
// TODO(vadimt): Decide what to do in incognito mode.
// TODO(vadimt): Gather UMAs.
// TODO(vadimt): Honor the flag the enables Google Now integration.
// TODO(vadimt): Figure out the final values of the constants.
// TODO(vadimt): Report internal and server errors. Collect UMAs on errors where
// appropriate. Also consider logging errors or throwing exceptions.
// TODO(vadimt): Consider processing errors for all storage.set calls.

// TODO(vadimt): Figure out the server name. Use it in the manifest and for
// NOTIFICATION_CARDS_URL. Meanwhile, to use the feature, you need to manually
// set the server name via local storage.
/**
 * URL to retrieve notification cards.
 */
var NOTIFICATION_CARDS_URL = localStorage['server_url'];

/**
 * Standard response code for successful HTTP requests. This is the only success
 * code the server will send.
 */
var HTTP_OK = 200;

/**
 * Initial period for polling for Google Now Notifications cards to use when the
 * period from the server is not available.
 */
var INITIAL_POLLING_PERIOD_SECONDS = 5 * 60;  // 5 minutes

/**
 * Maximal period for polling for Google Now Notifications cards to use when the
 * period from the server is not available.
 */
var MAXIMUM_POLLING_PERIOD_SECONDS = 60 * 60;  // 1 hour

var UPDATE_NOTIFICATIONS_ALARM_NAME = 'UPDATE';

/**
 * Period for retrying the server request for dismissing cards.
 */
var RETRY_DISMISS_PERIOD_SECONDS = 60;  // 1 minute

var RETRY_DISMISS_ALARM_NAME = 'RETRY_DISMISS';

/**
 * Time we keep dismissals after successful server dismiss requests.
 */
var DISMISS_RETENTION_TIME_MS = 20 * 60 * 1000;  // 20 minutes

var storage = chrome.storage.local;

/**
 * Names for tasks that can be created by the extension.
 */
var UPDATE_CARDS_TASK_NAME = 'update-cards';
var DISMISS_CARD_TASK_NAME = 'dismiss-card';
var CARD_CLICKED_TASK_NAME = 'card-clicked';
var RETRY_DISMISS_TASK_NAME = 'retry-dismiss';

/**
 * Checks if a new task can't be scheduled when another task is already
 * scheduled.
 * @param {string} newTaskName Name of the new task.
 * @param {string} scheduledTaskName Name of the scheduled task.
 * @return {boolean} Whether the new task conflicts with the existing task.
 */
function areTasksConflicting(newTaskName, scheduledTaskName) {
  if (newTaskName == UPDATE_CARDS_TASK_NAME &&
      scheduledTaskName == UPDATE_CARDS_TASK_NAME) {
    // If a card update is requested while an old update is still scheduled, we
    // don't need the new update.
    return true;
  }

  if (newTaskName == RETRY_DISMISS_TASK_NAME &&
      (scheduledTaskName == UPDATE_CARDS_TASK_NAME ||
       scheduledTaskName == DISMISS_CARD_TASK_NAME ||
       scheduledTaskName == RETRY_DISMISS_TASK_NAME)) {
    // No need to schedule retry-dismiss action if another action that tries to
    // send dismissals is scheduled.
    return true;
  }

  return false;
}

var tasks = buildTaskManager(areTasksConflicting);

/**
 * Shows a notification and remembers information associated with it.
 * @param {Object} card Google Now card represented as a set of parameters for
 *     showing a Chrome notification.
 * @param {Object} notificationsUrlInfo Map from notification id to the
 *     notification's set of URLs.
 */
function createNotification(card, notificationsUrlInfo) {
  // Create a notification or quietly update if it already exists.
  // TODO(vadimt): Implement non-quiet updates.
  chrome.notifications.create(
      card.notificationId,
      card.notification,
      function() {});

  notificationsUrlInfo[card.notificationId] = card.actionUrls;
}

/**
 * Parses JSON response from the notification server, show notifications and
 * schedule next update.
 * @param {string} response Server response.
 * @param {function()} callback Completion callback.
 */
function parseAndShowNotificationCards(response, callback) {
  try {
    var parsedResponse = JSON.parse(response);
  } catch (error) {
    // TODO(vadimt): Report errors to the user.
    return;
  }

  var cards = parsedResponse.cards;

  if (!(cards instanceof Array)) {
    // TODO(vadimt): Report errors to the user.
    return;
  }

  if (typeof parsedResponse.expiration_timestamp_seconds != 'number') {
    // TODO(vadimt): Report errors to the user.
    return;
  }

  tasks.debugSetStepName('parseAndShowNotificationCards-storage-get');
  storage.get(['activeNotifications', 'recentDismissals'], function(items) {
    // Build a set of non-expired recent dismissals. It will be used for
    // client-side filtering of cards.
    var updatedRecentDismissals = {};
    var currentTimeMs = Date.now();

    for (var notificationId in items.recentDismissals) {
      if (currentTimeMs - items.recentDismissals[notificationId] <
          DISMISS_RETENTION_TIME_MS) {
        updatedRecentDismissals[notificationId] =
            items.recentDismissals[notificationId];
      }
    }

    // Mark existing notifications that received an update in this server
    // response.
    for (var i = 0; i < cards.length; ++i) {
      var notificationId = cards[i].notificationId;
      if (!(notificationId in updatedRecentDismissals) &&
          notificationId in items.activeNotifications) {
        items.activeNotifications[notificationId].hasUpdate = true;
      }
    }

    // Delete notifications that didn't receive an update.
    for (var notificationId in items.activeNotifications) {
      if (!items.activeNotifications[notificationId].hasUpdate) {
        chrome.notifications.clear(
            notificationId,
            function() {});
      }
    }

    // Create/update notifications and store their new properties.
    var notificationsUrlInfo = {};

    for (var i = 0; i < cards.length; ++i) {
      var card = cards[i];
      if (!(card.notificationId in updatedRecentDismissals)) {
        try {
          createNotification(card, notificationsUrlInfo);
        } catch (error) {
          // TODO(vadimt): Report errors to the user.
        }
      }
    }

    scheduleNextUpdate(parsedResponse.expiration_timestamp_seconds);

    // Now that we got a valid response from the server, reset the retry period
    // to the initial value. This retry period will be used the next time we
    // fail to get the server-provided period.
    storage.set({
      retryDelaySeconds: INITIAL_POLLING_PERIOD_SECONDS,
      activeNotifications: notificationsUrlInfo,
      recentDismissals: updatedRecentDismissals
    });
    callback();
  });
}

/**
 * Requests notification cards from the server.
 * @param {string} requestParameters Query string for the request.
 * @param {function()} callback Completion callback.
 */
function requestNotificationCards(requestParameters, callback) {
  // TODO(vadimt): Figure out how to send user's identity to the server.
  var request = new XMLHttpRequest();

  request.responseType = 'text';
  request.onloadend = function(event) {
    if (request.status == HTTP_OK)
      parseAndShowNotificationCards(request.response, callback);
    else
      callback();
  };

  request.open(
      'GET',
      NOTIFICATION_CARDS_URL + '/notifications' + requestParameters,
      true);
  tasks.debugSetStepName('requestNotificationCards-send-request');
  request.send();
}

/**
 * Requests notification cards from the server when we have geolocation.
 * @param {Geoposition} position Location of this computer.
 * @param {function()} callback Completion callback.
 */
function requestNotificationCardsWithLocation(position, callback) {
  // TODO(vadimt): Should we use 'q' as the parameter name?
  var requestParameters =
      '?q=' + position.coords.latitude +
      ',' + position.coords.longitude +
      ',' + position.coords.accuracy;

  requestNotificationCards(requestParameters, callback);
}

/**
 * Obtains new location; requests and shows notification cards based on this
 * location.
 */
function updateNotificationsCards() {
  tasks.add(UPDATE_CARDS_TASK_NAME, function(callback) {
    tasks.debugSetStepName('updateNotificationsCards-get-retryDelaySeconds');
    storage.get('retryDelaySeconds', function(items) {
      // Immediately schedule the update after the current retry period. Then,
      // we'll use update time from the server if available.
      scheduleNextUpdate(items.retryDelaySeconds);

      // TODO(vadimt): Consider interrupting waiting for the next update if we
      // detect that the network conditions have changed. Also, decide whether
      // the exponential backoff is needed both when we are offline and when
      // there are failures on the server side.
      var newRetryDelaySeconds =
          Math.min(items.retryDelaySeconds * 2 * (1 + 0.2 * Math.random()),
                   MAXIMUM_POLLING_PERIOD_SECONDS);
      storage.set({retryDelaySeconds: newRetryDelaySeconds});

      processPendingDismissals(function(success) {
        if (success) {
          // The cards are requested only if there are no unsent dismissals.
          tasks.debugSetStepName('updateNotificationsCards-get-location');
          navigator.geolocation.getCurrentPosition(
            function(position) {
              requestNotificationCardsWithLocation(position, callback);
            },
            function() {
              requestNotificationCards('', callback);
            });
        } else {
          callback();
        }
      });
    });
  });
}

/**
 * Sends a server request to dismiss a card.
 * @param {string} notificationId Unique identifier of the card.
 * @param {function(boolean)} callbackBoolean Completion callback with 'success'
 *     parameter.
 */
function requestCardDismissal(notificationId, callbackBoolean) {
  // Send a dismiss request to the server.
  var requestParameters = '?id=' + notificationId;
  var request = new XMLHttpRequest();
  request.responseType = 'text';
  request.onloadend = function(event) {
    callbackBoolean(request.status == HTTP_OK);
  };

  request.open(
      'GET',
      NOTIFICATION_CARDS_URL + '/dismiss' + requestParameters,
      true);
  tasks.debugSetStepName('requestCardDismissal-send-request');
  request.send();
}

/**
 * Tries to send dismiss requests for all pending dismissals.
 * @param {function(boolean)} callbackBoolean Completion callback with 'success'
 *     parameter. Success means that no pending dismissals are left.
 */
function processPendingDismissals(callbackBoolean) {
  tasks.debugSetStepName('processPendingDismissals-storage-get');
  storage.get(['pendingDismissals', 'recentDismissals'], function(items) {
    var dismissalsChanged = false;

    function onFinish(success) {
      if (dismissalsChanged) {
        storage.set({
          pendingDismissals: items.pendingDismissals,
          recentDismissals: items.recentDismissals
        });
      }
      callbackBoolean(success);
    }

    function doProcessDismissals() {
      if (items.pendingDismissals.length == 0) {
        chrome.alarms.clear(RETRY_DISMISS_ALARM_NAME);
        onFinish(true);
        return;
      }

      // Send dismissal for the first card, and if successful, repeat
      // recursively with the rest.
      var notificationId = items.pendingDismissals[0];
      requestCardDismissal(notificationId, function(success) {
        if (success) {
          dismissalsChanged = true;
          items.pendingDismissals.splice(0, 1);
          items.recentDismissals[notificationId] = Date.now();
          doProcessDismissals();
        } else {
          onFinish(false);
        }
      });
    }

    doProcessDismissals();
  });
}

/**
 * Submits a task to send pending dismissals.
 */
function retryPendingDismissals() {
  tasks.add(RETRY_DISMISS_TASK_NAME, function(callback) {
    processPendingDismissals(function(success) { callback(); });
  });
}

/**
 * Opens URL corresponding to the clicked part of the notification.
 * @param {string} notificationId Unique identifier of the notification.
 * @param {function(Object): string} selector Function that extracts the url for
 *     the clicked area from the button action URLs info.
 */
function onNotificationClicked(notificationId, selector) {
  tasks.add(CARD_CLICKED_TASK_NAME, function(callback) {
    tasks.debugSetStepName('onNotificationClicked-get-activeNotifications');
    storage.get('activeNotifications', function(items) {
      var actionUrls = items.activeNotifications[notificationId];
      if (typeof actionUrls != 'object') {
        // TODO(vadimt): report an error.
        callback();
        return;
      }

      var url = selector(actionUrls);

      if (typeof url != 'string') {
        // TODO(vadimt): report an error.
        callback();
        return;
      }

      chrome.tabs.create({url: url});
      callback();
    });
  });
}

/**
 * Callback for chrome.notifications.onClosed event.
 * @param {string} notificationId Unique identifier of the notification.
 * @param {boolean} byUser Whether the notification was closed by the user.
 */
function onNotificationClosed(notificationId, byUser) {
  if (!byUser)
    return;

  tasks.add(DISMISS_CARD_TASK_NAME, function(callback) {
    // Schedule retrying dismissing until all dismissals go through.
    // TODO(vadimt): Implement exponential backoff and unify it with getting
    // cards.
    var alarmInfo = {
      delayInMinutes: RETRY_DISMISS_PERIOD_SECONDS / 60,
      periodInMinutes: RETRY_DISMISS_PERIOD_SECONDS / 60
    };

    chrome.alarms.create(RETRY_DISMISS_ALARM_NAME, alarmInfo);

    // Deleting the notification in case it was re-added while this task was
    // scheduled, waiting for execution.
    chrome.notifications.clear(
        notificationId,
        function() {});

    tasks.debugSetStepName('onNotificationClosed-get-pendingDismissals');
    storage.get('pendingDismissals', function(items) {
      items.pendingDismissals.push(notificationId);
      storage.set({pendingDismissals: items.pendingDismissals});
      processPendingDismissals(function(success) { callback(); });
    });
  });
}

/**
 * Schedules next update for notification cards.
 * @param {int} delaySeconds Length of time in seconds after which the alarm
 *     event should fire.
 */
function scheduleNextUpdate(delaySeconds) {
  // Schedule an alarm after the specified delay. 'periodInMinutes' is for the
  // case when we fail to re-register the alarm.
  var alarmInfo = {
    delayInMinutes: delaySeconds / 60,
    periodInMinutes: MAXIMUM_POLLING_PERIOD_SECONDS / 60
  };

  chrome.alarms.create(UPDATE_NOTIFICATIONS_ALARM_NAME, alarmInfo);
}

/**
 * Initializes the event page on install or on browser startup.
 */
function initialize() {
  var initialStorage = {
    activeNotifications: {},
    recentDismissals: {},
    retryDelaySeconds: INITIAL_POLLING_PERIOD_SECONDS
  };
  storage.set(initialStorage);
  updateNotificationsCards();
}

chrome.runtime.onInstalled.addListener(function(details) {
  if (details.reason != 'chrome_update') {
    storage.set({pendingDismissals: []});
    initialize();
  }
});

chrome.runtime.onStartup.addListener(function() {
  initialize();
});

chrome.alarms.onAlarm.addListener(function(alarm) {
  if (alarm.name == UPDATE_NOTIFICATIONS_ALARM_NAME)
    updateNotificationsCards();
  else if (alarm.name == RETRY_DISMISS_ALARM_NAME)
    retryPendingDismissals();
});

chrome.notifications.onClicked.addListener(
    function(notificationId) {
      onNotificationClicked(notificationId, function(actionUrls) {
        return actionUrls.messageUrl;
      });
    });

chrome.notifications.onButtonClicked.addListener(
    function(notificationId, buttonIndex) {
      onNotificationClicked(notificationId, function(actionUrls) {
        if (!Array.isArray(actionUrls.buttonUrls))
          return undefined;

        return actionUrls.buttonUrls[buttonIndex];
      });
    });

chrome.notifications.onClosed.addListener(onNotificationClosed);
