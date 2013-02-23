// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview The event page for Google Now for Chrome implementation.
 * The Google Now event page gets Google Now cards from the server and shows
 * them as Chrome notifications.
 * The service performs periodic updating of Google Now cards.
 * Each updating of the cards includes 3 steps:
 * 1. Obtaining the location of the machine;
 * 2. Making a server request based on that location;
 * 3. Showing the received cards as notifications.
 */

// TODO(vadimt): Use background permission to show notifications even when all
// browser windows are closed.
// TODO(vadimt): Remove the C++ implementation.
// TODO(vadimt): Decide what to do in incognito mode.
// TODO(vadimt): Gather UMAs.
// TODO(vadimt): Honor the flag the enables Google Now integration.
// TODO(vadimt): Figure out the final values of the constants.
// TODO(vadimt): Report errors to the user.

// TODO(vadimt): Figure out the server name. Use it in the manifest and for
// TODO(vadimt): Consider processing errors for all storage.set calls.
// NOTIFICATION_CARDS_URL. Meanwhile, to use the feature, you need to manually
// edit NOTIFICATION_CARDS_URL before building Chrome.
/**
 * URL to retrieve notification cards.
 */
var NOTIFICATION_CARDS_URL = '';

/**
 * Standard response code for successful HTTP requests. This is the only success
 * code the server will send.
 */
var HTTP_OK = 200;

/**
 * Initial period for polling for Google Now Notifications cards to use when the
 * period from the server is not available.
 */
var INITIAL_POLLING_PERIOD_SECONDS = 300;  // 5 minutes

/**
 * Maximal period for polling for Google Now Notifications cards to use when the
 * period from the server is not available.
 */
var MAXIMUM_POLLING_PERIOD_SECONDS = 3600;  // 1 hour

var storage = chrome.storage.local;

/**
 * Show a notification and remember information associated with it.
 * @param {Object} card Google Now card represented as a set of parameters for
 *     showing a Chrome notification.
 * @param {Object} notificationsUrlInfo Map from notification id to the
 *     notification's set of URLs.
 */
function createNotification(card, notificationsUrlInfo) {
  var notificationId = card.notificationId;

  // Fill the information about button actions for the notification. This is a
  // map from the clicked area name to URL to open when the area is clicked. Any
  // of the fields may be defined or undefined.
  var actionUrls = {
    message: card.messageUrl,
    button0: card.buttonOneUrl,
    button1: card.buttonTwoUrl
  };

  // TODO(vadimt): Reorganize server response to avoid deleting fields.
  delete card.notificationId;
  delete card.messageUrl;
  delete card.buttonOneUrl;
  delete card.buttonTwoUrl;

  // Create a notification or quietly update if it already exists.
  // TODO(vadimt): Implement non-quiet updates.
  chrome.experimental.notification.create(
      notificationId,
      card,
      function(assignedNotificationId) {});

  notificationsUrlInfo[notificationId] = actionUrls;
}

/**
 * Parse JSON response from the notification server, show notifications and
 * schedule next update.
 * @param {string} response Server response.
 */
function parseAndShowNotificationCards(response) {
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

  storage.get('activeNotifications', function(items) {
    // Mark existing notifications that received an update in this server
    // response.
    for (var i = 0; i < cards.length; ++i) {
      var notificationId = cards[i].notificationId;
      if (notificationId in items.activeNotifications)
        items.activeNotifications[notificationId].hasUpdate = true;
    }

    // Delete notifications that didn't receive an update.
    for (var notificationId in items.activeNotifications)
      if (!items.activeNotifications[notificationId].hasUpdate) {
        chrome.experimental.notification.delete(
            notificationId,
            function(wasDeleted) {});
    }

    // Create/update notifications and store their new properties.
    var notificationsUrlInfo = {};

    for (var i = 0; i < cards.length; ++i) {
      try {
        createNotification(cards[i], notificationsUrlInfo);
      } catch (error) {
        // TODO(vadimt): Report errors to the user.
      }
    }
    storage.set({activeNotifications: notificationsUrlInfo});

    scheduleNextUpdate(parsedResponse.expiration_timestamp_seconds);

    // Now that we got a valid response from the server, reset the retry period
    // to the initial value. This retry period will be used the next time we
    // fail to get the server-provided period.
    storage.set({retryDelaySeconds: INITIAL_POLLING_PERIOD_SECONDS});
  });
}

/**
 * Request notification cards from the server.
 * @param {string} requestParameters Query string for the request.
 */
function requestNotificationCards(requestParameters) {
  // TODO(vadimt): Figure out how to send user's identity to the server.
  var request = new XMLHttpRequest();

  request.responseType = 'text';
  request.onload = function(event) {
    if (request.status == HTTP_OK)
      parseAndShowNotificationCards(request.response);
  }

  request.open(
      'GET',
      NOTIFICATION_CARDS_URL + '/notifications' + requestParameters,
      true);
  request.send();
}

/**
 * Request notification cards from the server when we have geolocation.
 * @param {Geoposition} position Location of this computer.
 */
function requestNotificationCardsWithLocation(position) {
  // TODO(vadimt): Should we use 'q' as the parameter name?
  var requestParameters =
      '?q=' + position.coords.latitude +
      ',' + position.coords.longitude +
      ',' + position.coords.accuracy;

  requestNotificationCards(requestParameters);
}

/**
 * Request notification cards from the server when we don't have geolocation.
 * @param {PositionError} positionError Position error.
 */
function requestNotificationCardsWithoutLocation(positionError) {
  requestNotificationCards('');
}

/**
 * Obtain new location; request and show notification cards based on this
 * location.
 */
function updateNotificationsCards() {
  storage.get('retryDelaySeconds', function(items) {
    // Immediately schedule the update after the current retry period. Then,
    // we'll use update time from the server if available.
    scheduleNextUpdate(items.retryDelaySeconds);

    // TODO(vadimt): Consider interrupting waiting for the next update if we
    // detect that the network conditions have changed. Also, decide whether the
    // exponential backoff is needed both when we are offline and when there are
    // failures on the server side.
    var newRetryDelaySeconds =
        Math.min(items.retryDelaySeconds * 2 * (1 + 0.2 * Math.random()),
                 MAXIMUM_POLLING_PERIOD_SECONDS);
    storage.set({retryDelaySeconds: newRetryDelaySeconds});

    navigator.geolocation.getCurrentPosition(
        requestNotificationCardsWithLocation,
        requestNotificationCardsWithoutLocation);
  });
}

/**
 * Opens URL corresponding to the clicked part of the notification.
 * @param {string} notificationId Unique identifier of the notification.
 * @param {string} area Name of the notification's clicked area.
 */
function onNotificationClicked(notificationId, area) {
  storage.get('activeNotifications', function(items) {
    var notificationActions = items.activeNotifications[notificationId] || {};
    if (area in notificationActions) {
      // Open URL specified for the clicked area.
      // TODO(vadimt): Figure out whether to open link in a new tab etc.
      chrome.windows.create({url: notificationActions[area]});
    }
  });
}

/**
 * Callback for chrome.experimental.notification.onClosed event.
 * @param {string} notificationId Unique identifier of the notification.
 * @param {boolean} byUser Whether the notification was closed by the user.
 */
function onNotificationClosed(notificationId, byUser) {
  if (byUser) {
    // TODO(vadimt): Analyze possible race conditions between request for cards
    // and dismissal.
    // Send a dismiss request to the server.
    var requestParameters = '?id=' + notificationId;
    var request = new XMLHttpRequest();
    request.responseType = 'text';
    // TODO(vadimt): If the request fails, for example, because there is no
    // internet connection, do retry with exponential backoff.
    request.open(
      'GET',
      NOTIFICATION_CARDS_URL + '/dismiss' + requestParameters,
      true);
    request.send();
  }
}

/**
 * Schedule next update for notification cards.
 * @param {int} delaySeconds Length of time in seconds after which the alarm
 *     event should fire.
 */
function scheduleNextUpdate(delaySeconds) {
  // Schedule an alarm after the specified delay. 'periodInMinutes' is for the
  // case when we fail to re-register the alarm.
  chrome.alarms.create({
    delayInMinutes: delaySeconds / 60,
    periodInMinutes: MAXIMUM_POLLING_PERIOD_SECONDS / 60
  });
}

/**
 * Initialize the event page on install or on browser startup.
 */
function initialize() {
  var initialStorage = {
    activeNotifications: {},
    retryDelaySeconds: INITIAL_POLLING_PERIOD_SECONDS
  };
  storage.set(initialStorage, updateNotificationsCards);
}

chrome.runtime.onInstalled.addListener(function(details) {
  if (details.reason != 'chrome_update')
    initialize();
});

chrome.runtime.onStartup.addListener(function() {
  initialize();
});

chrome.alarms.onAlarm.addListener(function(alarm) {
  updateNotificationsCards();
});

chrome.experimental.notification.onClicked.addListener(
    function(notificationId) {
      onNotificationClicked(notificationId, 'message');
    });

chrome.experimental.notification.onButtonClicked.addListener(
    function(notificationId, buttonIndex) {
      onNotificationClicked(notificationId, 'button' + buttonIndex);
    });

chrome.experimental.notification.onClosed.addListener(onNotificationClosed);
