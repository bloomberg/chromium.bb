// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Show/hide trigger in a card.
 *
 * @typedef {{
 *   showTime: number=,
 *   hideTime: number=
 * }}
 */
var Trigger;

/**
 * Data to build a dismissal request for a card from a specific group.
 *
 * @typedef {{
 *   notificationId: string,
 *   parameters: Object
 * }}
 */
var DismissalData;

/**
 * Card merged from potentially multiple groups.
 *
 * @typedef {{
 *   trigger: Trigger,
 *   version: number,
 *   timestamp: number,
 *   notification: Object,
 *   actionUrls: Object=,
 *   groupRank: number,
 *   dismissals: Array.<DismissalData>
 * }}
 */
var MergedCard;

/**
 * Set of parameters for creating card notification.
 *
 * @typedef {{
 *   notification: Object,
 *   hideTime: number=,
 *   version: number,
 *   previousVersion: number=
 * }}
 */
var CardCreateInfo;

/**
 * Names for tasks that can be created by the this file.
 */
var CLEAR_CARD_TASK_NAME = 'clear-card';

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
   * @param {CardCreateInfo} cardCreateInfo Google Now card represented as a set
   *     of parameters for showing a Chrome notification.
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

            scheduleHiding(cardId, cardCreateInfo.hideTime);
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

            scheduleHiding(cardId, cardCreateInfo.hideTime);
          });
    }
  }

  /**
   * Updates/creates a card notification with new data.
   * @param {string} cardId Card ID.
   * @param {MergedCard} card Google Now card from the server.
   * @param {number=} previousVersion The version of the shown card with
   *     this id, if it exists, undefined otherwise.
   * @return {Object} Notification data entry for this card.
   */
  function update(cardId, card, previousVersion) {
    console.log('cardManager.update ' + JSON.stringify(card) + ' ' +
        previousVersion);

    chrome.alarms.clear(cardHidePrefix + cardId);

    var cardCreateInfo = {
      notification: card.notification,
      hideTime: card.trigger.hideTime,
      version: card.version,
      previousVersion: previousVersion
    };

    var cardShowAlarmName = cardShowPrefix + cardId;
    if (card.trigger.showTime && card.trigger.showTime > Date.now()) {
      // Card needs to be shown later.
      console.log('cardManager.update: postponed');
      var alarmInfo = {
        when: card.trigger.showTime
      };
      chrome.alarms.create(cardShowAlarmName, alarmInfo);
    } else {
      // Card needs to be shown immediately.
      console.log('cardManager.update: immediate');
      chrome.alarms.clear(cardShowAlarmName);
      showNotification(cardId, cardCreateInfo);
    }

    return {
      actionUrls: card.actionUrls,
      cardCreateInfo: cardCreateInfo,
      dismissals: card.dismissals
    };
  }

  /**
   * Removes a card notification.
   * @param {string} cardId Card ID.
   * @param {boolean} clearStorage True if the information associated with the
   *     card should be erased from chrome.storage.
   */
  function clear(cardId, clearStorage) {
    console.log('cardManager.clear ' + cardId);

    chrome.notifications.clear(cardId, function() {});
    chrome.alarms.clear(cardShowPrefix + cardId);
    chrome.alarms.clear(cardHidePrefix + cardId);

    if (clearStorage) {
      instrumented.storage.local.get(
          ['notificationsData', 'notificationGroups'],
          function(items) {
            items = items || {};
            items.notificationsData = items.notificationsData || {};
            items.notificationGroups = items.notificationGroups || {};

            delete items.notificationsData[cardId];

            for (var groupName in items.notificationGroups) {
              var group = items.notificationGroups[groupName];
              for (var i = 0; i != group.cards.length; ++i) {
                if (group.cards[i].chromeNotificationId == cardId) {
                  group.cards.splice(i, 1);
                  break;
                }
              }
            }

            chrome.storage.local.set(items);
          });
    }
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
      tasks.add(CLEAR_CARD_TASK_NAME, function() {
        var cardId = alarm.name.substring(cardHidePrefix.length);
        clear(cardId, true);
      });
    }
  });

  return {
    update: update,
    clear: clear
  };
}
