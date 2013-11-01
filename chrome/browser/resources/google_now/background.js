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
 * 1. Obtaining the location of the machine;
 * 2. Processing requests for cards dismissals that are not yet sent to the
 *    server;
 * 3. Making a server request based on that location;
 * 4. Showing the received cards as notifications.
 */

// TODO(vadimt): Decide what to do in incognito mode.
// TODO(vadimt): Figure out the final values of the constants.
// TODO(vadimt): Remove 'console' calls.

/**
 * Standard response code for successful HTTP requests. This is the only success
 * code the server will send.
 */
var HTTP_OK = 200;
var HTTP_NOCONTENT = 204;

var HTTP_BAD_REQUEST = 400;
var HTTP_UNAUTHORIZED = 401;
var HTTP_FORBIDDEN = 403;
var HTTP_METHOD_NOT_ALLOWED = 405;

var MS_IN_SECOND = 1000;
var MS_IN_MINUTE = 60 * 1000;

/**
 * Initial period for polling for Google Now Notifications cards to use when the
 * period from the server is not available.
 */
var INITIAL_POLLING_PERIOD_SECONDS = 5 * 60;  // 5 minutes

/**
 * Mininal period for polling for Google Now Notifications cards.
 */
var MINIMUM_POLLING_PERIOD_SECONDS = 5 * 60;  // 5 minutes

/**
 * Maximal period for polling for Google Now Notifications cards to use when the
 * period from the server is not available.
 */
var MAXIMUM_POLLING_PERIOD_SECONDS = 60 * 60;  // 1 hour

/**
 * Initial period for retrying the server request for dismissing cards.
 */
var INITIAL_RETRY_DISMISS_PERIOD_SECONDS = 60;  // 1 minute

/**
 * Maximum period for retrying the server request for dismissing cards.
 */
var MAXIMUM_RETRY_DISMISS_PERIOD_SECONDS = 60 * 60;  // 1 hour

/**
 * Time we keep retrying dismissals.
 */
var MAXIMUM_DISMISSAL_AGE_MS = 24 * 60 * 60 * 1000; // 1 day

/**
 * Time we keep dismissals after successful server dismiss requests.
 */
var DISMISS_RETENTION_TIME_MS = 20 * 60 * 1000;  // 20 minutes

/**
 * Default period for checking whether the user is opted in to Google Now.
 */
var DEFAULT_OPTIN_CHECK_PERIOD_SECONDS = 60 * 60 * 24 * 7; // 1 week

/**
 * URL to open when the user clicked on a link for the our notification
 * settings.
 */
var SETTINGS_URL = 'https://support.google.com/chrome/?p=ib_google_now_welcome';

/**
 * Names for tasks that can be created by the extension.
 */
var UPDATE_CARDS_TASK_NAME = 'update-cards';
var DISMISS_CARD_TASK_NAME = 'dismiss-card';
var RETRY_DISMISS_TASK_NAME = 'retry-dismiss';
var STATE_CHANGED_TASK_NAME = 'state-changed';
var SHOW_ON_START_TASK_NAME = 'show-cards-on-start';
var ON_PUSH_MESSAGE_START_TASK_NAME = 'on-push-message';

var LOCATION_WATCH_NAME = 'location-watch';

/**
 * Notification as it's sent by the server.
 *
 * @typedef {{
 *   notificationId: string,
 *   chromeNotificationId: string,
 *   trigger: Object=,
 *   version: number,
 *   chromeNotificationOptions: Object,
 *   actionUrls: Object=,
 *   dismissal: Object
 * }}
 */
var UnmergedNotification;

/**
 * Notification group as the client stores it. |cardsTimestamp| and |rank| are
 * defined if |cards| is non-empty. |nextPollTime| is undefined if the server
 * (1) never sent 'nextPollSeconds' for the group or
 * (2) didn't send 'nextPollSeconds' with the last group update containing a
 *     cards update and all the times after that.
 *
 * @typedef {{
 *   cards: Array.<UnmergedNotification>,
 *   cardsTimestamp: number=,
 *   nextPollTime: number=,
 *   rank: number=
 * }}
 */
var StorageGroup;

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

// Add error processing to API calls.
wrapper.instrumentChromeApiFunction('location.onLocationUpdate.addListener', 0);
wrapper.instrumentChromeApiFunction('metricsPrivate.getVariationParams', 1);
wrapper.instrumentChromeApiFunction('notifications.clear', 1);
wrapper.instrumentChromeApiFunction('notifications.create', 2);
wrapper.instrumentChromeApiFunction('notifications.getPermissionLevel', 0);
wrapper.instrumentChromeApiFunction('notifications.update', 2);
wrapper.instrumentChromeApiFunction('notifications.getAll', 0);
wrapper.instrumentChromeApiFunction(
    'notifications.onButtonClicked.addListener', 0);
wrapper.instrumentChromeApiFunction('notifications.onClicked.addListener', 0);
wrapper.instrumentChromeApiFunction('notifications.onClosed.addListener', 0);
wrapper.instrumentChromeApiFunction(
    'notifications.onPermissionLevelChanged.addListener', 0);
wrapper.instrumentChromeApiFunction(
    'notifications.onShowSettings.addListener', 0);
wrapper.instrumentChromeApiFunction(
    'preferencesPrivate.googleGeolocationAccessEnabled.get',
    1);
wrapper.instrumentChromeApiFunction(
    'preferencesPrivate.googleGeolocationAccessEnabled.onChange.addListener',
    0);
wrapper.instrumentChromeApiFunction('permissions.contains', 1);
wrapper.instrumentChromeApiFunction('pushMessaging.onMessage.addListener', 0);
wrapper.instrumentChromeApiFunction('runtime.onInstalled.addListener', 0);
wrapper.instrumentChromeApiFunction('runtime.onStartup.addListener', 0);
wrapper.instrumentChromeApiFunction('tabs.create', 1);
wrapper.instrumentChromeApiFunction('storage.local.get', 1);

var updateCardsAttempts = buildAttemptManager(
    'cards-update',
    requestLocation,
    INITIAL_POLLING_PERIOD_SECONDS,
    MAXIMUM_POLLING_PERIOD_SECONDS);
var dismissalAttempts = buildAttemptManager(
    'dismiss',
    retryPendingDismissals,
    INITIAL_RETRY_DISMISS_PERIOD_SECONDS,
    MAXIMUM_RETRY_DISMISS_PERIOD_SECONDS);
var cardSet = buildCardSet();

var authenticationManager = buildAuthenticationManager();

/**
 * Google Now UMA event identifier.
 * @enum {number}
 */
var GoogleNowEvent = {
  REQUEST_FOR_CARDS_TOTAL: 0,
  REQUEST_FOR_CARDS_SUCCESS: 1,
  CARDS_PARSE_SUCCESS: 2,
  DISMISS_REQUEST_TOTAL: 3,
  DISMISS_REQUEST_SUCCESS: 4,
  LOCATION_REQUEST: 5,
  LOCATION_UPDATE: 6,
  EXTENSION_START: 7,
  DELETED_SHOW_WELCOME_TOAST: 8,
  STOPPED: 9,
  DELETED_USER_SUPPRESSED: 10,
  EVENTS_TOTAL: 11  // EVENTS_TOTAL is not an event; all new events need to be
                    // added before it.
};

/**
 * Records a Google Now Event.
 * @param {GoogleNowEvent} event Event identifier.
 */
function recordEvent(event) {
  var metricDescription = {
    metricName: 'GoogleNow.Event',
    type: 'histogram-linear',
    min: 1,
    max: GoogleNowEvent.EVENTS_TOTAL,
    buckets: GoogleNowEvent.EVENTS_TOTAL + 1
  };

  chrome.metricsPrivate.recordValue(metricDescription, event);
}

/**
 * Adds authorization behavior to the request.
 * @param {XMLHttpRequest} request Server request.
 * @param {function(boolean)} callbackBoolean Completion callback with 'success'
 *     parameter.
 */
function setAuthorization(request, callbackBoolean) {
  authenticationManager.getAuthToken(function(token) {
    if (!token) {
      callbackBoolean(false);
      return;
    }

    request.setRequestHeader('Authorization', 'Bearer ' + token);

    // Instrument onloadend to remove stale auth tokens.
    var originalOnLoadEnd = request.onloadend;
    request.onloadend = wrapper.wrapCallback(function(event) {
      if (request.status == HTTP_FORBIDDEN ||
          request.status == HTTP_UNAUTHORIZED) {
        authenticationManager.removeToken(token, function() {
          originalOnLoadEnd(event);
        });
      } else {
        originalOnLoadEnd(event);
      }
    });

    callbackBoolean(true);
  });
}

/**
 * Shows parsed and merged cards as notifications.
 * @param {Object.<string, MergedCard>} cards Map from chromeNotificationId to
 *     the merged card, containing cards to show.
 */
function showNotificationCards(cards) {
  console.log('showNotificationCards ' + JSON.stringify(cards));

  instrumented.storage.local.get(['notificationsData', 'recentDismissals'],
      function(items) {
        console.log('showNotificationCards-get ' +
            JSON.stringify(items));
        items = items || {};
        items.notificationsData = items.notificationsData || {};
        items.recentDismissals = items.recentDismissals || {};

        instrumented.notifications.getAll(function(notifications) {
          console.log('showNotificationCards-getAll ' +
              JSON.stringify(notifications));
          notifications = notifications || {};

          // Build a set of non-expired recent dismissals. It will be used for
          // client-side filtering of cards.
          var updatedRecentDismissals = {};
          var currentTimeMs = Date.now();
          for (var chromeNotificationId in items.recentDismissals) {
            if (currentTimeMs - items.recentDismissals[chromeNotificationId] <
                DISMISS_RETENTION_TIME_MS) {
              updatedRecentDismissals[chromeNotificationId] =
                  items.recentDismissals[chromeNotificationId];
              delete cards[chromeNotificationId];
            }
          }

          // Delete notifications that didn't receive an update.
          for (var chromeNotificationId in notifications) {
            console.log('showNotificationCards-delete-check ' +
                        chromeNotificationId);
            if (!(chromeNotificationId in cards)) {
              console.log(
                  'showNotificationCards-delete ' + chromeNotificationId);
              cardSet.clear(chromeNotificationId, false);
            }
          }

          // Create/update notifications and store their new properties.
          var newNotificationsData = {};
          for (var chromeNotificationId in cards) {
            var notificationData =
                items.notificationsData[chromeNotificationId];
            var previousVersion = notifications[chromeNotificationId] &&
                                  notificationData &&
                                  notificationData.cardCreateInfo &&
                                  notificationData.cardCreateInfo.version;
            newNotificationsData[chromeNotificationId] = cardSet.update(
                chromeNotificationId,
                cards[chromeNotificationId],
                previousVersion);
          }

          chrome.storage.local.set({
            notificationsData: newNotificationsData,
            recentDismissals: updatedRecentDismissals
          });
        });
      });
}

/**
 * Removes all cards and card state on Google Now close down.
 * For example, this occurs when the geolocation preference is unchecked in the
 * content settings.
 */
function removeAllCards() {
  console.log('removeAllCards');

  // TODO(robliao): Once Google Now clears its own checkbox in the
  // notifications center and bug 260376 is fixed, the below clearing
  // code is no longer necessary.
  instrumented.notifications.getAll(function(notifications) {
    notifications = notifications || {};
    for (var chromeNotificationId in notifications) {
      instrumented.notifications.clear(chromeNotificationId, function() {});
    }
    chrome.storage.local.remove(['notificationsData', 'notificationGroups']);
  });
}

/**
 * Merges an unmerged notification into a merged card with same ID.
 * @param {MergedCard=} mergedCard Existing merged card or undefined if a merged
 *     card doesn't exist (i.e. we see this ID for the first time while
 *     merging).
 * @param {UnmergedNotification} unmergedNotification Notification as it was
 *     received from the server.
 * @param {number} notificationTimestamp The moment the unmerged card was
 *     received.
 * @param {number} notificationGroupRank Rank of the group of the unmerged card.
 * @return {MergedCard} Result of merging |unmergedNotification| into
 *     |mergedCard|.
 */
function mergeCards(
    mergedCard,
    unmergedNotification,
    notificationTimestamp,
    notificationGroupRank) {
  var result = mergedCard || {dismissals: []};

  var priority = mergedCard ?
      Math.max(
          mergedCard.notification.priority,
          unmergedNotification.chromeNotificationOptions.priority) :
      unmergedNotification.chromeNotificationOptions.priority;

  if (!mergedCard || notificationGroupRank > mergedCard.groupRank) {
    result.groupRank = notificationGroupRank;
    var showTime = unmergedNotification.trigger &&
        unmergedNotification.trigger.showTimeSec &&
        (notificationTimestamp +
         unmergedNotification.trigger.showTimeSec * MS_IN_SECOND);
    var hideTime = unmergedNotification.trigger &&
        unmergedNotification.trigger.hideTimeSec &&
        (notificationTimestamp +
         unmergedNotification.trigger.hideTimeSec * MS_IN_SECOND);
    result.trigger = {
      showTime: showTime,
      hideTime: hideTime
    };
  }

  if (!mergedCard || notificationTimestamp > mergedCard.timestamp) {
    result.timestamp = notificationTimestamp;
    result.notification = unmergedNotification.chromeNotificationOptions;
    result.actionUrls = unmergedNotification.actionUrls;
    result.version = unmergedNotification.version;
  }

  result.notification.priority = priority;
  var dismissalData = {
    notificationId: unmergedNotification.notificationId,
    parameters: unmergedNotification.dismissal
  };
  result.dismissals.push(dismissalData);

  return result;
}

/**
 * Merges a card group into a set of merged cards.
 * @param {Object.<string, MergedCard>} mergedCards Map from
 *     chromeNotificationId to a merged card.
 *     This is an input/output parameter.
 * @param {StorageGroup} storageGroup Group to merge into the merged card set.
 */
function mergeGroup(mergedCards, storageGroup) {
  for (var i = 0; i < storageGroup.cards.length; i++) {
    var card = storageGroup.cards[i];
    mergedCards[card.chromeNotificationId] = mergeCards(
        mergedCards[card.chromeNotificationId],
        card,
        storageGroup.cardsTimestamp,
        storageGroup.rank);
  }
}

/**
 * Schedules next cards poll.
 * @param {Object.<string, StorageGroup>} groups Map from group name to group
 *     information.
 * @param {boolean} isOptedIn True if the user is opted in to Google Now.
 */
function scheduleNextPoll(groups, isOptedIn) {
  if (isOptedIn) {
    var nextPollTime = null;

    for (var groupName in groups) {
      var group = groups[groupName];
      if (group.nextPollTime !== undefined) {
        nextPollTime = nextPollTime == null ?
            group.nextPollTime : Math.min(group.nextPollTime, nextPollTime);
      }
    }

    // At least one of the groups must have nextPollTime.
    verify(nextPollTime != null, 'scheduleNextPoll: nextPollTime is null');

    var nextPollDelaySeconds = Math.max(
        (nextPollTime - Date.now()) / MS_IN_SECOND,
        MINIMUM_POLLING_PERIOD_SECONDS);
    updateCardsAttempts.start(nextPollDelaySeconds);
  } else {
    instrumented.metricsPrivate.getVariationParams(
        'GoogleNow', function(params) {
      var optinPollPeriodSeconds =
          parseInt(params && params.optinPollPeriodSeconds, 10) ||
          DEFAULT_OPTIN_CHECK_PERIOD_SECONDS;
      updateCardsAttempts.start(optinPollPeriodSeconds);
    });
  }
}

/**
 * Merges notification groups into a set of Chrome notifications and shows them.
 * @param {Object.<string, StorageGroup>} notificationGroups Map from group name
 *     to group information.
 */
function mergeAndShowNotificationCards(notificationGroups) {
  var mergedCards = {};

  for (var groupName in notificationGroups)
    mergeGroup(mergedCards, notificationGroups[groupName]);

  showNotificationCards(mergedCards);
}

/**
 * Parses JSON response from the notification server, shows notifications and
 * schedules next update.
 * @param {string} response Server response.
 */
function parseAndShowNotificationCards(response) {
  console.log('parseAndShowNotificationCards ' + response);
  var parsedResponse = JSON.parse(response);

  if (parsedResponse.googleNowDisabled) {
    chrome.storage.local.set({googleNowEnabled: false});
    // TODO(vadimt): Remove the line below once the server stops sending groups
    // with 'googleNowDisabled' responses.
    parsedResponse.groups = {};
    // Google Now was enabled; now it's disabled. This is a state change.
    onStateChange();
  }

  var receivedGroups = parsedResponse.groups;

  // Populate groups with corresponding cards.
  if (parsedResponse.notifications) {
    for (var i = 0; i != parsedResponse.notifications.length; ++i) {
      var card = parsedResponse.notifications[i];
      var group = receivedGroups[card.groupName];
      group.cards = group.cards || [];
      group.cards.push(card);
    }
  }

  instrumented.storage.local.get('notificationGroups', function(items) {
    console.log('parseAndShowNotificationCards-get ' + JSON.stringify(items));
    items = items || {};
    items.notificationGroups = items.notificationGroups || {};

    var now = Date.now();

    // Build updated set of groups.
    var updatedGroups = {};

    for (var groupName in receivedGroups) {
      var receivedGroup = receivedGroups[groupName];
      var storageGroup = items.notificationGroups[groupName] || {
        cards: [],
        cardsTimestamp: undefined,
        nextPollTime: undefined,
        rank: undefined
      };

      if (receivedGroup.requested)
        receivedGroup.cards = receivedGroup.cards || [];

      if (receivedGroup.cards) {
        // If the group contains a cards update, all its fields will get new
        // values.
        storageGroup.cards = receivedGroup.cards;
        storageGroup.cardsTimestamp = now;
        storageGroup.rank = receivedGroup.rank;
        storageGroup.nextPollTime = undefined;
        // The code below assigns nextPollTime a defined value if
        // nextPollSeconds is specified in the received group.
        // If the group's cards are not updated, and nextPollSeconds is
        // unspecified, this method doesn't change group's nextPollTime.
      }

      // 'nextPollSeconds' may be sent even for groups that don't contain cards
      // updates.
      if (receivedGroup.nextPollSeconds !== undefined) {
        storageGroup.nextPollTime =
            now + receivedGroup.nextPollSeconds * MS_IN_SECOND;
      }

      updatedGroups[groupName] = storageGroup;
    }

    scheduleNextPoll(updatedGroups, !parsedResponse.googleNowDisabled);
    chrome.storage.local.set({notificationGroups: updatedGroups});
    mergeAndShowNotificationCards(updatedGroups);
    recordEvent(GoogleNowEvent.CARDS_PARSE_SUCCESS);
  });
}

/**
 * Requests notification cards from the server for specified groups.
 * @param {Array.<string>} groupNames Names of groups that need to be refreshed.
 */
function requestNotificationGroups(groupNames) {
  console.log('requestNotificationGroups from ' + NOTIFICATION_CARDS_URL +
      ', groupNames=' + JSON.stringify(groupNames));

  recordEvent(GoogleNowEvent.REQUEST_FOR_CARDS_TOTAL);

  var requestParameters = '?timeZoneOffsetMs=' +
      (-new Date().getTimezoneOffset() * MS_IN_MINUTE);

  groupNames.forEach(function(groupName) {
    requestParameters += ('&requestTypes=' + groupName);
  });

  console.log('requestNotificationGroups: request=' + requestParameters);

  var request = buildServerRequest('GET', 'notifications' + requestParameters);

  request.onloadend = function(event) {
    console.log('requestNotificationGroups-onloadend ' + request.status);
    if (request.status == HTTP_OK) {
      recordEvent(GoogleNowEvent.REQUEST_FOR_CARDS_SUCCESS);
      parseAndShowNotificationCards(request.response);
    }
  };

  setAuthorization(request, function(success) {
    if (success)
      request.send();
  });
}

/**
 * Requests the account opted-in state from the server.
 * @param {function()} optedInCallback Function that will be called if
 *     opted-in state is 'true'.
 */
function requestOptedIn(optedInCallback) {
  console.log('requestOptedIn from ' + NOTIFICATION_CARDS_URL);

  var request = buildServerRequest('GET', 'settings/optin');

  request.onloadend = function(event) {
    console.log(
        'requestOptedIn-onloadend ' + request.status + ' ' + request.response);
    if (request.status == HTTP_OK) {
      var parsedResponse = JSON.parse(request.response);
      if (parsedResponse.value) {
        chrome.storage.local.set({googleNowEnabled: true});
        optedInCallback();
        // Google Now was disabled, now it's enabled. This is a state change.
        onStateChange();
      } else {
        scheduleNextPoll({}, false);
      }
    }
  };

  setAuthorization(request, function(success) {
    if (success)
      request.send();
  });
}

/**
 * Requests notification cards from the server.
 * @param {Location} position Location of this computer.
 */
function requestNotificationCards(position) {
  console.log('requestNotificationCards ' + JSON.stringify(position));

  instrumented.storage.local.get(
      ['notificationGroups', 'googleNowEnabled'], function(items) {
    console.log('requestNotificationCards-storage-get ' +
                JSON.stringify(items));
    items = items || {};

    var groupsToRequest = [];

    if (items.notificationGroups) {
      var now = Date.now();

      for (var groupName in items.notificationGroups) {
        var group = items.notificationGroups[groupName];
        if (group.nextPollTime !== undefined && group.nextPollTime <= now)
          groupsToRequest.push(groupName);
      }
    }

    if (items.googleNowEnabled) {
      requestNotificationGroups(groupsToRequest);
    } else {
      requestOptedIn(function() {
        requestNotificationGroups(groupsToRequest);
      });
    }
  });
}

/**
 * Starts getting location for a cards update.
 */
function requestLocation() {
  console.log('requestLocation');
  recordEvent(GoogleNowEvent.LOCATION_REQUEST);
  // TODO(vadimt): Figure out location request options.
  instrumented.metricsPrivate.getVariationParams('GoogleNow', function(params) {
    var minDistanceInMeters =
        parseInt(params && params.minDistanceInMeters, 10) ||
        100;
    var minTimeInMilliseconds =
        parseInt(params && params.minTimeInMilliseconds, 10) ||
        180000;  // 3 minutes.

    // TODO(vadimt): Uncomment/remove watchLocation and remove invoking
    // updateNotificationsCards once state machine design is finalized.
//    chrome.location.watchLocation(LOCATION_WATCH_NAME, {
//      minDistanceInMeters: minDistanceInMeters,
//      minTimeInMilliseconds: minTimeInMilliseconds
//    });
    // We need setTimeout to avoid recursive task creation. This is a temporary
    // code, and it will be removed once we finally decide to send or not send
    // client location to the server.
    setTimeout(wrapper.wrapCallback(updateNotificationsCards, true), 0);
  });
}

/**
 * Stops getting the location.
 */
function stopRequestLocation() {
  console.log('stopRequestLocation');
  chrome.location.clearWatch(LOCATION_WATCH_NAME);
}

/**
 * Obtains new location; requests and shows notification cards based on this
 * location.
 * @param {Location} position Location of this computer.
 */
function updateNotificationsCards(position) {
  console.log('updateNotificationsCards ' + JSON.stringify(position) +
      ' @' + new Date());
  tasks.add(UPDATE_CARDS_TASK_NAME, function() {
    console.log('updateNotificationsCards-task-begin');
    updateCardsAttempts.isRunning(function(running) {
      if (running) {
        updateCardsAttempts.planForNext(function() {
          processPendingDismissals(function(success) {
            if (success) {
              // The cards are requested only if there are no unsent dismissals.
              requestNotificationCards(position);
            }
          });
        });
      }
    });
  });
}

/**
 * Sends a server request to dismiss a card.
 * @param {string} chromeNotificationId chrome.notifications ID of the card.
 * @param {number} dismissalTimeMs Time of the user's dismissal of the card in
 *     milliseconds since epoch.
 * @param {DismissalData} dismissalData Data to build a dismissal request.
 * @param {function(boolean)} callbackBoolean Completion callback with 'done'
 *     parameter.
 */
function requestCardDismissal(
    chromeNotificationId, dismissalTimeMs, dismissalData, callbackBoolean) {
  console.log('requestDismissingCard ' + chromeNotificationId + ' from ' +
      NOTIFICATION_CARDS_URL);

  var dismissalAge = Date.now() - dismissalTimeMs;

  if (dismissalAge > MAXIMUM_DISMISSAL_AGE_MS) {
    callbackBoolean(true);
    return;
  }

  recordEvent(GoogleNowEvent.DISMISS_REQUEST_TOTAL);

  var request = 'notifications/' + dismissalData.notificationId +
      '?age=' + dismissalAge +
      '&chromeNotificationId=' + chromeNotificationId;

  for (var paramField in dismissalData.parameters)
    request += ('&' + paramField + '=' + dismissalData.parameters[paramField]);

  console.log('requestCardDismissal: request=' + request);

  var request = buildServerRequest('DELETE', request);
  request.onloadend = function(event) {
    console.log('requestDismissingCard-onloadend ' + request.status);
    if (request.status == HTTP_NOCONTENT)
      recordEvent(GoogleNowEvent.DISMISS_REQUEST_SUCCESS);

    // A dismissal doesn't require further retries if it was successful or
    // doesn't have a chance for successful completion.
    var done = request.status == HTTP_NOCONTENT ||
        request.status == HTTP_BAD_REQUEST ||
        request.status == HTTP_METHOD_NOT_ALLOWED;
    callbackBoolean(done);
  };

  setAuthorization(request, function(success) {
    if (success)
      request.send();
    else
      callbackBoolean(false);
  });
}

/**
 * Tries to send dismiss requests for all pending dismissals.
 * @param {function(boolean)} callbackBoolean Completion callback with 'success'
 *     parameter. Success means that no pending dismissals are left.
 */
function processPendingDismissals(callbackBoolean) {
  instrumented.storage.local.get(['pendingDismissals', 'recentDismissals'],
      function(items) {
        console.log('processPendingDismissals-storage-get ' +
                    JSON.stringify(items));
        items = items || {};
        items.pendingDismissals = items.pendingDismissals || [];
        items.recentDismissals = items.recentDismissals || {};

        var dismissalsChanged = false;

        function onFinish(success) {
          if (dismissalsChanged) {
            chrome.storage.local.set({
              pendingDismissals: items.pendingDismissals,
              recentDismissals: items.recentDismissals
            });
          }
          callbackBoolean(success);
        }

        function doProcessDismissals() {
          if (items.pendingDismissals.length == 0) {
            dismissalAttempts.stop();
            onFinish(true);
            return;
          }

          // Send dismissal for the first card, and if successful, repeat
          // recursively with the rest.
          var dismissal = items.pendingDismissals[0];
          requestCardDismissal(
              dismissal.chromeNotificationId,
              dismissal.time,
              dismissal.dismissalData,
              function(done) {
                if (done) {
                  dismissalsChanged = true;
                  items.pendingDismissals.splice(0, 1);
                  items.recentDismissals[dismissal.chromeNotificationId] =
                      Date.now();
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
  tasks.add(RETRY_DISMISS_TASK_NAME, function() {
    dismissalAttempts.planForNext(function() {
      processPendingDismissals(function(success) {});
     });
  });
}

/**
 * Opens a URL in a new tab.
 * @param {string} url URL to open.
 */
function openUrl(url) {
  instrumented.tabs.create({url: url}, function(tab) {
    if (tab)
      chrome.windows.update(tab.windowId, {focused: true});
    else
      chrome.windows.create({url: url, focused: true});
  });
}

/**
 * Opens URL corresponding to the clicked part of the notification.
 * @param {string} chromeNotificationId chrome.notifications ID of the card.
 * @param {function(Object): string} selector Function that extracts the url for
 *     the clicked area from the button action URLs info.
 */
function onNotificationClicked(chromeNotificationId, selector) {
  instrumented.storage.local.get('notificationsData', function(items) {
    var notificationData = items &&
        items.notificationsData &&
        items.notificationsData[chromeNotificationId];

    if (!notificationData)
      return;

    var url = selector(notificationData.actionUrls);
    if (!url)
      return;

    openUrl(url);
  });
}

/**
 * Callback for chrome.notifications.onClosed event.
 * @param {string} chromeNotificationId chrome.notifications ID of the card.
 * @param {boolean} byUser Whether the notification was closed by the user.
 */
function onNotificationClosed(chromeNotificationId, byUser) {
  if (!byUser)
    return;

  // At this point we are guaranteed that the notification is a now card.
  chrome.metricsPrivate.recordUserAction('GoogleNow.Dismissed');

  tasks.add(DISMISS_CARD_TASK_NAME, function() {
    dismissalAttempts.start();

    instrumented.storage.local.get(
        ['pendingDismissals', 'notificationsData'], function(items) {
      items = items || {};
      items.pendingDismissals = items.pendingDismissals || [];
      items.notificationsData = items.notificationsData || {};

      // Deleting the notification in case it was re-added while this task was
      // scheduled, waiting for execution; also cleaning notification's data
      // from storage.
      cardSet.clear(chromeNotificationId, true);

      var notificationData = items.notificationsData[chromeNotificationId];

      if (notificationData && notificationData.dismissals) {
        for (var i = 0; i < notificationData.dismissals.length; i++) {
          var dismissal = {
            chromeNotificationId: chromeNotificationId,
            time: Date.now(),
            dismissalData: notificationData.dismissals[i]
          };
          items.pendingDismissals.push(dismissal);
        }

        chrome.storage.local.set({pendingDismissals: items.pendingDismissals});
      }

      processPendingDismissals(function(success) {});
    });
  });
}

/**
 * Initializes the polling system to start monitoring location and fetching
 * cards.
 */
function startPollingCards() {
  // Create an update timer for a case when for some reason location request
  // gets stuck.
  updateCardsAttempts.start(MAXIMUM_POLLING_PERIOD_SECONDS);

  requestLocation();
}

/**
 * Stops all machinery in the polling system.
 */
function stopPollingCards() {
  stopRequestLocation();
  updateCardsAttempts.stop();
  removeAllCards();
  // Mark the Google Now as disabled to start with checking the opt-in state
  // next time startPollingCards() is called.
  chrome.storage.local.set({googleNowEnabled: false});
}

/**
 * Initializes the event page on install or on browser startup.
 */
function initialize() {
  recordEvent(GoogleNowEvent.EXTENSION_START);
  onStateChange();
}

/**
 * Starts or stops the polling of cards.
 * @param {boolean} shouldPollCardsRequest true to start and
 *     false to stop polling cards.
 */
function setShouldPollCards(shouldPollCardsRequest) {
  updateCardsAttempts.isRunning(function(currentValue) {
    if (shouldPollCardsRequest != currentValue) {
      console.log('Action Taken setShouldPollCards=' + shouldPollCardsRequest);
      if (shouldPollCardsRequest)
        startPollingCards();
      else
        stopPollingCards();
    } else {
      console.log(
          'Action Ignored setShouldPollCards=' + shouldPollCardsRequest);
    }
  });
}

/**
 * Enables or disables the Google Now background permission.
 * @param {boolean} backgroundEnable true to run in the background.
 *     false to not run in the background.
 */
function setBackgroundEnable(backgroundEnable) {
  instrumented.permissions.contains({permissions: ['background']},
      function(hasPermission) {
        if (backgroundEnable != hasPermission) {
          console.log('Action Taken setBackgroundEnable=' + backgroundEnable);
          if (backgroundEnable)
            chrome.permissions.request({permissions: ['background']});
          else
            chrome.permissions.remove({permissions: ['background']});
        } else {
          console.log('Action Ignored setBackgroundEnable=' + backgroundEnable);
        }
      });
}

/**
 * Does the actual work of deciding what Google Now should do
 * based off of the current state of Chrome.
 * @param {boolean} signedIn true if the user is signed in.
 * @param {boolean} geolocationEnabled true if
 *     the geolocation option is enabled.
 * @param {boolean} canEnableBackground true if
 *     the background permission can be requested.
 * @param {boolean} notificationEnabled true if
 *     Google Now for Chrome is allowed to show notifications.
 * @param {boolean} googleNowEnabled true if
 *     the Google Now is enabled for the user.
 */
function updateRunningState(
    signedIn,
    geolocationEnabled,
    canEnableBackground,
    notificationEnabled,
    googleNowEnabled) {
  console.log(
      'State Update signedIn=' + signedIn + ' ' +
      'geolocationEnabled=' + geolocationEnabled + ' ' +
      'canEnableBackground=' + canEnableBackground + ' ' +
      'notificationEnabled=' + notificationEnabled + ' ' +
      'googleNowEnabled=' + googleNowEnabled);

  // TODO(vadimt): Remove this line once state machine design is finalized.
  geolocationEnabled = true;

  var shouldPollCards = false;
  var shouldSetBackground = false;

  if (signedIn && notificationEnabled) {
    if (geolocationEnabled) {
      if (canEnableBackground && googleNowEnabled)
        shouldSetBackground = true;

      shouldPollCards = true;
    }
  } else {
    recordEvent(GoogleNowEvent.STOPPED);
  }

  console.log(
      'Requested Actions shouldSetBackground=' + shouldSetBackground + ' ' +
      'setShouldPollCards=' + shouldPollCards);

  setBackgroundEnable(shouldSetBackground);
  setShouldPollCards(shouldPollCards);
}

/**
 * Coordinates the behavior of Google Now for Chrome depending on
 * Chrome and extension state.
 */
function onStateChange() {
  tasks.add(STATE_CHANGED_TASK_NAME, function() {
    authenticationManager.isSignedIn(function(signedIn) {
      instrumented.metricsPrivate.getVariationParams(
          'GoogleNow',
          function(response) {
            var canEnableBackground =
                (!response || (response.canEnableBackground != 'false'));
            instrumented.notifications.getPermissionLevel(function(level) {
              var notificationEnabled = (level == 'granted');
              instrumented.
                preferencesPrivate.
                googleGeolocationAccessEnabled.
                get({}, function(prefValue) {
                  var geolocationEnabled = !!prefValue.value;
                  instrumented.storage.local.get(
                      'googleNowEnabled',
                      function(items) {
                        var googleNowEnabled =
                            items && !!items.googleNowEnabled;
                        updateRunningState(
                            signedIn,
                            geolocationEnabled,
                            canEnableBackground,
                            notificationEnabled,
                            googleNowEnabled);
                      });
                });
            });
          });
    });
  });
}

instrumented.runtime.onInstalled.addListener(function(details) {
  console.log('onInstalled ' + JSON.stringify(details));
  if (details.reason != 'chrome_update') {
    initialize();
  }
});

instrumented.runtime.onStartup.addListener(function() {
  console.log('onStartup');

  // Show notifications received by earlier polls. Doing this as early as
  // possible to reduce latency of showing first notifications. This mimics how
  // persistent notifications will work.
  tasks.add(SHOW_ON_START_TASK_NAME, function() {
    instrumented.storage.local.get('notificationGroups', function(items) {
      console.log('onStartup-get ' + JSON.stringify(items));
      items = items || {};
      items.notificationGroups = items.notificationGroups || {};

      mergeAndShowNotificationCards(items.notificationGroups);
    });
  });

  initialize();
});

instrumented.
    preferencesPrivate.
    googleGeolocationAccessEnabled.
    onChange.
    addListener(function(prefValue) {
      console.log('googleGeolocationAccessEnabled Pref onChange ' +
          prefValue.value);
      onStateChange();
});

authenticationManager.addListener(function() {
  console.log('signIn State Change');
  onStateChange();
});

instrumented.notifications.onClicked.addListener(
    function(chromeNotificationId) {
      chrome.metricsPrivate.recordUserAction('GoogleNow.MessageClicked');
      onNotificationClicked(chromeNotificationId, function(actionUrls) {
        return actionUrls && actionUrls.messageUrl;
      });
    });

instrumented.notifications.onButtonClicked.addListener(
    function(chromeNotificationId, buttonIndex) {
      chrome.metricsPrivate.recordUserAction(
          'GoogleNow.ButtonClicked' + buttonIndex);
      onNotificationClicked(chromeNotificationId, function(actionUrls) {
        var url = actionUrls.buttonUrls[buttonIndex];
        verify(url, 'onButtonClicked: no url for a button');
        return url;
      });
    });

instrumented.notifications.onClosed.addListener(onNotificationClosed);

instrumented.notifications.onPermissionLevelChanged.addListener(
    function(permissionLevel) {
      console.log('Notifications permissionLevel Change');
      onStateChange();
    });

instrumented.notifications.onShowSettings.addListener(function() {
  openUrl(SETTINGS_URL);
});

instrumented.location.onLocationUpdate.addListener(function(position) {
  recordEvent(GoogleNowEvent.LOCATION_UPDATE);
  updateNotificationsCards(position);
});

instrumented.pushMessaging.onMessage.addListener(function(message) {
  // message.payload will be '' when the extension first starts.
  // Each time after signing in, we'll get latest payload for all channels.
  // So, we need to poll the server only when the payload is non-empty and has
  // changed.
  console.log('pushMessaging.onMessage ' + JSON.stringify(message));
  if (message.payload.indexOf('REQUEST_CARDS') == 0) {
    tasks.add(ON_PUSH_MESSAGE_START_TASK_NAME, function() {
      instrumented.storage.local.get(
          ['lastPollNowPayloads', 'notificationGroups'], function(items) {
        // If storage.get fails, it's safer to do nothing, preventing polling
        // the server when the payload really didn't change.
        if (!items)
          return;

        // If this is the first time we get lastPollNowPayloads, initialize it.
        items.lastPollNowPayloads = items.lastPollNowPayloads || {};

        if (items.lastPollNowPayloads[message.subchannelId] !=
            message.payload) {
          items.lastPollNowPayloads[message.subchannelId] = message.payload;

          items.notificationGroups = items.notificationGroups || {};
          items.notificationGroups['PUSH' + message.subchannelId] = {
            cards: [],
            nextPollTime: Date.now()
          };

          chrome.storage.local.set({
            lastPollNowPayloads: items.lastPollNowPayloads,
            notificationGroups: items.notificationGroups
          });

          updateNotificationsCards();
        }
      });
    });
  }
});
