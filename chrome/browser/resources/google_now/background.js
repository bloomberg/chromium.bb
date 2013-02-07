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
 * Period for polling for Google Now Notifications cards to use when the period
 * from the server is not available.
 */
var DEFAULT_POLLING_PERIOD_SECONDS = 300;  // 5 minutes

/**
 * Parse JSON response of the notifications server, show notifications and
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

  for (var i = 0; i != cards.length; ++i) {
    try {
      var card = cards[i];
      // TODO(vadimt): Use URLs for buttons.
      delete card.buttonOneIntent;
      delete card.buttonTwoIntent;
      chrome.experimental.notification.show(card, function(showInfo) {});
    } catch (error) {
      // TODO(vadimt): Report errors to the user.
      return;
    }
  }

  scheduleNextUpdate(parsedResponse.expiration_timestamp_seconds);
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
  // Immediately schedule the update after the default period. If we
  // successfully retrieve, parse and show the notifications cards, we'll
  // schedule next update based on the expiration timestamp received from the
  // server. At that point scheduled time will be overwritten by the new one
  // based on the expiration timestamp.
  // TODO(vadimt): Implement exponential backoff with randomized jitter.
  scheduleNextUpdate(DEFAULT_POLLING_PERIOD_SECONDS);

  // TODO(vadimt): Use chrome.* geolocation API once it's ready.
  navigator.geolocation.getCurrentPosition(
      requestNotificationCardsWithLocation,
      requestNotificationCardsWithoutLocation);
}

/**
* Callback for chrome.experimental.notification.onClosed event.
* @param {string} replaceId Replace ID of the notification.
* @param {boolean} byUser Flag indicating whether the notification was closed by
*     the user.
*/
function onNotificationClosed(replaceId, byUser) {
  if (byUser) {
    // Send a dismiss request to the server.
    var requestParameters = '?id=' + replaceId;
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
  chrome.alarms.create({delayInMinutes: delaySeconds / 60});
}

/**
 * Initialize the event page on install or on browser startup.
 */
function initialize() {
  updateNotificationsCards();
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

chrome.experimental.notification.onClosed.addListener(onNotificationClosed);
