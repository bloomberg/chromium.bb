// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var MS_IN_SECOND = 1000;

/**
 * Builds an object to manage notification card set.
 * @return {Object} Card set interface.
 */
function buildCardSet() {
  var cardShowPrefix = 'card-show-';
  var cardHidePrefix = 'card-hide-';

  /**
   * Schedules hiding a notification.
   * @param {string} cardId Card ID.
   * @param {number=} opt_timeHide If specified, epoch time to hide the card. If
   *      undefined, the card will be kept shown at least until next update.
   */
  function scheduleHiding(cardId, opt_timeHide) {
    if (opt_timeHide === undefined)
      return;

    var alarmName = cardHidePrefix + cardId;
    var alarmInfo = {when: opt_timeHide};
    chrome.alarms.create(alarmName, alarmInfo);
  }

  /**
   * Shows a notification.
   * @param {string} cardId Card ID.
   * @param {Object} cardCreateInfo Google Now card represented as a set of
   *     parameters for showing a Chrome notification.
   */
  function showNotification(cardId, cardCreateInfo) {
    console.log('cardManager.showNotification ' + cardId + ' ' +
                JSON.stringify(cardCreateInfo));

    if (cardCreateInfo.previousVersion !== cardCreateInfo.version) {
      // Delete a notification with the specified id if it already exists, and
      // then create a notification.
      instrumented.notifications.create(
          cardId,
          cardCreateInfo.notification,
          function(newNotificationId) {
            if (!newNotificationId || chrome.runtime.lastError) {
              var errorMessage = chrome.runtime.lastError &&
                                  chrome.runtime.lastError.message;
              console.error('notifications.create: ID=' + newNotificationId +
                            ', ERROR=' + errorMessage);
              return;
            }

            scheduleHiding(cardId, cardCreateInfo.timeHide);
          });
    } else {
      // Update existing notification.
      instrumented.notifications.update(
          cardId,
          cardCreateInfo.notification,
          function(wasUpdated) {
            if (!wasUpdated || chrome.runtime.lastError) {
              var errorMessage = chrome.runtime.lastError &&
                                  chrome.runtime.lastError.message;
              console.error('notifications.update: UPDATED=' + wasUpdated +
                            ', ERROR=' + errorMessage);
              return;
            }

            scheduleHiding(cardId, cardCreateInfo.timeHide);
          });
    }
  }

  /**
   * Updates/creates a card notification with new data.
   * @param {Object} card Google Now from the server.
   * @param {number=} previousVersion The version of the shown card with
   *     this id, if it exists, undefined otherwise.
   * @return {Object} Notification data entry for this card.
   */
  function update(card, previousVersion) {
    console.log('cardManager.update ' + JSON.stringify(card) + ' ' +
        previousVersion);

    if (typeof card.version != 'number') {
      console.log('cardCreateInfo.version is not a number');
      // Fix card version.
      card.version = previousVersion || 0;
    }

    // TODO(vadimt): Don't clear alarms etc that don't exist. Or make sure doing
    // this doesn't output an error to console.
    chrome.alarms.clear(cardHidePrefix + card.notificationId);

    var timeHide = card.trigger && card.trigger.hideTimeSec !== undefined ?
        Date.now() + card.trigger.hideTimeSec * MS_IN_SECOND : undefined;
    var cardCreateInfo = {
      notification: card.notification,
      timeHide: timeHide,
      version: card.version,
      previousVersion: previousVersion
    };

    var cardShowAlarmName = cardShowPrefix + card.notificationId;
    if (card.trigger && card.trigger.showTimeSec) {
      // Card needs to be shown later.
      console.log('cardManager.register: postponed');
      var alarmInfo = {
        when: Date.now() + card.trigger.showTimeSec * MS_IN_SECOND
      };
      chrome.alarms.create(cardShowAlarmName, alarmInfo);
    } else {
      // Card needs to be shown immediately.
      console.log('cardManager.register: immediate');
      chrome.alarms.clear(cardShowAlarmName);
      showNotification(card.notificationId, cardCreateInfo);
    }

    return {
      actionUrls: card.actionUrls,
      cardCreateInfo: cardCreateInfo,
      dismissalParameters: card.dismissal
    };
  }

  /**
   * Removes a card notification.
   * @param {string} cardId Card ID.
   */
  function clear(cardId) {
    console.log('cardManager.unregister ' + cardId);

    chrome.notifications.clear(cardId, function() {});
    chrome.alarms.clear(cardShowPrefix + cardId);
    chrome.alarms.clear(cardHidePrefix + cardId);
  }

  instrumented.alarms.onAlarm.addListener(function(alarm) {
    console.log('cardManager.onAlarm ' + JSON.stringify(alarm));

    if (alarm.name.indexOf(cardShowPrefix) == 0) {
      // Alarm to show the card.
      var cardId = alarm.name.substring(cardShowPrefix.length);
      instrumented.storage.local.get('notificationsData', function(items) {
        console.log('cardManager.onAlarm.get ' + JSON.stringify(items));
        if (!items || !items.notificationsData)
          return;
        var notificationData = items.notificationsData[cardId];
        if (!notificationData)
          return;

        showNotification(cardId, notificationData.cardCreateInfo);
      });
    } else if (alarm.name.indexOf(cardHidePrefix) == 0) {
      // Alarm to hide the card.
      var cardId = alarm.name.substring(cardHidePrefix.length);
      chrome.notifications.clear(cardId, function() {});
    }
  });

  return {
    update: update,
    clear: clear
  };
}
