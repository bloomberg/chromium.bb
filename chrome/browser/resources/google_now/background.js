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

// TODO(vadimt): Use background permission to show notifications even when all
// browser windows are closed.
// TODO(vadimt): Decide what to do in incognito mode.
// TODO(vadimt): Honor the flag the enables Google Now integration.
// TODO(vadimt): Figure out the final values of the constants.
// TODO(vadimt): Remove 'console' calls.
// TODO(vadimt): Consider sending JS stacks for chrome.* API errors and
// malformed server responses.

/**
 * Standard response code for successful HTTP requests. This is the only success
 * code the server will send.
 */
var HTTP_OK = 200;

var HTTP_UNAUTHORIZED = 401;
var HTTP_FORBIDDEN = 403;

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

/**
 * Initial period for retrying the server request for dismissing cards.
 */
var INITIAL_RETRY_DISMISS_PERIOD_SECONDS = 60;  // 1 minute

/**
 * Maximum period for retrying the server request for dismissing cards.
 */
var MAXIMUM_RETRY_DISMISS_PERIOD_SECONDS = 60 * 60;  // 1 hour

/**
 * Time we keep dismissals after successful server dismiss requests.
 */
var DISMISS_RETENTION_TIME_MS = 20 * 60 * 1000;  // 20 minutes

/**
 * Names for tasks that can be created by the extension.
 */
var UPDATE_CARDS_TASK_NAME = 'update-cards';
var DISMISS_CARD_TASK_NAME = 'dismiss-card';
var RETRY_DISMISS_TASK_NAME = 'retry-dismiss';

var LOCATION_WATCH_NAME = 'location-watch';

var WELCOME_TOAST_NOTIFICATION_ID = 'enable-now-toast';

/**
 * The indices of the buttons that are displayed on the welcome toast.
 * @enum {number}
 */
var ToastButtonIndex = {YES: 0, NO: 1};

/**
 * The action that the user performed on the welcome toast.
 * @enum {number}
 */
var ToastOptionResponse = {CHOSE_YES: 1, CHOSE_NO: 2};

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
tasks.instrumentApiFunction(chrome.identity, 'getAuthToken', 1);
tasks.instrumentApiFunction(chrome.identity, 'removeCachedAuthToken', 1);
tasks.instrumentApiFunction(chrome.location.onLocationUpdate, 'addListener', 0);
tasks.instrumentApiFunction(chrome.notifications, 'create', 2);
tasks.instrumentApiFunction(chrome.notifications, 'update', 2);
tasks.instrumentApiFunction(chrome.notifications, 'getAll', 0);
tasks.instrumentApiFunction(
    chrome.notifications.onButtonClicked, 'addListener', 0);
tasks.instrumentApiFunction(chrome.notifications.onClicked, 'addListener', 0);
tasks.instrumentApiFunction(chrome.notifications.onClosed, 'addListener', 0);
tasks.instrumentApiFunction(chrome.runtime.onInstalled, 'addListener', 0);
tasks.instrumentApiFunction(chrome.runtime.onStartup, 'addListener', 0);
tasks.instrumentApiFunction(chrome.tabs, 'create', 1);
tasks.instrumentApiFunction(storage, 'get', 1);

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

/**
 * Diagnostic event identifier.
 * @enum {number}
 */
var DiagnosticEvent = {
  REQUEST_FOR_CARDS_TOTAL: 0,
  REQUEST_FOR_CARDS_SUCCESS: 1,
  CARDS_PARSE_SUCCESS: 2,
  DISMISS_REQUEST_TOTAL: 3,
  DISMISS_REQUEST_SUCCESS: 4,
  LOCATION_REQUEST: 5,
  LOCATION_UPDATE: 6,
  EVENTS_TOTAL: 7  // EVENTS_TOTAL is not an event; all new events need to be
                   // added before it.
};

/**
 * Records a diagnostic event.
 * @param {DiagnosticEvent} event Event identifier.
 */
function recordEvent(event) {
  var metricDescription = {
    metricName: 'GoogleNow.Event',
    type: 'histogram-linear',
    min: 1,
    max: DiagnosticEvent.EVENTS_TOTAL,
    buckets: DiagnosticEvent.EVENTS_TOTAL + 1
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
  tasks.debugSetStepName('setAuthorization-getAuthToken');
  chrome.identity.getAuthToken({interactive: false}, function(token) {
    var errorMessage =
        chrome.runtime.lastError && chrome.runtime.lastError.message;
    console.log('setAuthorization: error=' + errorMessage +
                ', token=' + (token && 'non-empty'));
    if (chrome.runtime.lastError || !token) {
      callbackBoolean(false);
      return;
    }

    request.setRequestHeader('Authorization', 'Bearer ' + token);

    // Instrument onloadend to remove stale auth tokens.
    var originalOnLoadEnd = request.onloadend;
    request.onloadend = tasks.wrapCallback(function(event) {
      if (request.status == HTTP_FORBIDDEN ||
          request.status == HTTP_UNAUTHORIZED) {
        tasks.debugSetStepName('setAuthorization-removeCachedAuthToken');
        chrome.identity.removeCachedAuthToken({token: token}, function() {
          // After purging the token cache, call getAuthToken() again to let
          // Chrome know about the problem with the token.
          chrome.identity.getAuthToken({interactive: false}, function() {});
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
 * Shows a notification and remembers information associated with it.
 * @param {Object} card Google Now card represented as a set of parameters for
 *     showing a Chrome notification.
 * @param {Object} notificationsData Map from notification id to the data
 *     associated with a notification.
 * @param {number=} opt_previousVersion The version of the shown card with this
 *     id, if it exists, undefined otherwise.
 */
function showNotification(card, notificationsData, opt_previousVersion) {
  console.log('showNotification ' + JSON.stringify(card) + ' ' +
             opt_previousVersion);

  if (typeof card.version != 'number') {
    console.error('card.version is not a number');
    // Fix card version.
    card.version = opt_previousVersion !== undefined ? opt_previousVersion : 0;
  }

  if (opt_previousVersion !== card.version) {
    try {
      // Delete a notification with the specified id if it already exists, and
      // then create a notification.
      chrome.notifications.create(
          card.notificationId,
          card.notification,
          function(notificationId) {
            if (!notificationId || chrome.runtime.lastError) {
              var errorMessage =
                  chrome.runtime.lastError && chrome.runtime.lastError.message;
              console.error('notifications.create: ID=' + notificationId +
                            ', ERROR=' + errorMessage);
            }
          });
    } catch (error) {
      console.error('Error in notifications.create: ' + error);
    }
  } else {
    try {
      // Update existing notification.
      chrome.notifications.update(
          card.notificationId,
          card.notification,
          function(wasUpdated) {
            if (!wasUpdated || chrome.runtime.lastError) {
              var errorMessage =
                  chrome.runtime.lastError && chrome.runtime.lastError.message;
              console.error('notifications.update: UPDATED=' + wasUpdated +
                            ', ERROR=' + errorMessage);
            }
          });
    } catch (error) {
      console.error('Error in notifications.update: ' + error);
    }
  }

  notificationsData[card.notificationId] = {
    actionUrls: card.actionUrls,
    version: card.version
  };
}

/**
 * Parses JSON response from the notification server, show notifications and
 * schedule next update.
 * @param {string} response Server response.
 * @param {function()} callback Completion callback.
 */
function parseAndShowNotificationCards(response, callback) {
  console.log('parseAndShowNotificationCards ' + response);
  try {
    var parsedResponse = JSON.parse(response);
  } catch (error) {
    console.error('parseAndShowNotificationCards parse error: ' + error);
    callback();
    return;
  }

  var cards = parsedResponse.cards;

  if (!(cards instanceof Array)) {
    callback();
    return;
  }

  if (typeof parsedResponse.expiration_timestamp_seconds != 'number') {
    callback();
    return;
  }

  tasks.debugSetStepName('parseAndShowNotificationCards-storage-get');
  storage.get(['notificationsData', 'recentDismissals'], function(items) {
    console.log('parseAndShowNotificationCards-get ' + JSON.stringify(items));
    items.notificationsData = items.notificationsData || {};
    items.recentDismissals = items.recentDismissals || {};

    tasks.debugSetStepName(
        'parseAndShowNotificationCards-notifications-getAll');
    chrome.notifications.getAll(function(notifications) {
      console.log('parseAndShowNotificationCards-getAll ' +
          JSON.stringify(notifications));
      // TODO(vadimt): Figure out what to do when notifications are disabled for
      // our extension.
      notifications = notifications || {};

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
      var updatedNotifications = {};

      for (var i = 0; i < cards.length; ++i) {
        var notificationId = cards[i].notificationId;
        if (!(notificationId in updatedRecentDismissals) &&
            notificationId in notifications) {
          updatedNotifications[notificationId] = true;
        }
      }

      // Delete notifications that didn't receive an update.
      for (var notificationId in notifications) {
        console.log('parseAndShowNotificationCards-delete-check ' +
                    notificationId);
        if (!(notificationId in updatedNotifications)) {
          console.log('parseAndShowNotificationCards-delete ' + notificationId);
          chrome.notifications.clear(
              notificationId,
              function() {});
        }
      }

      recordEvent(DiagnosticEvent.CARDS_PARSE_SUCCESS);

      // Create/update notifications and store their new properties.
      var newNotificationsData = {};
      for (var i = 0; i < cards.length; ++i) {
        var card = cards[i];
        if (!(card.notificationId in updatedRecentDismissals)) {
          var notificationData = items.notificationsData[card.notificationId];
          var previousVersion = notifications[card.notificationId] &&
                                notificationData &&
                                notificationData.previousVersion;
          showNotification(card, newNotificationsData, previousVersion);
        }
      }

      updateCardsAttempts.start(parsedResponse.expiration_timestamp_seconds);

      storage.set({
        notificationsData: newNotificationsData,
        recentDismissals: updatedRecentDismissals
      });
      callback();
    });
  });
}

/**
 * Requests notification cards from the server.
 * @param {Location} position Location of this computer.
 * @param {function()} callback Completion callback.
 */
function requestNotificationCards(position, callback) {
  console.log('requestNotificationCards ' + JSON.stringify(position) +
      ' from ' + NOTIFICATION_CARDS_URL);

  if (!NOTIFICATION_CARDS_URL) {
    callback();
    return;
  }

  recordEvent(DiagnosticEvent.REQUEST_FOR_CARDS_TOTAL);

  // TODO(vadimt): Should we use 'q' as the parameter name?
  var requestParameters =
      'q=' + position.coords.latitude +
      ',' + position.coords.longitude +
      ',' + position.coords.accuracy;

  var request = buildServerRequest('notifications');

  request.onloadend = function(event) {
    console.log('requestNotificationCards-onloadend ' + request.status);
    if (request.status == HTTP_OK) {
      recordEvent(DiagnosticEvent.REQUEST_FOR_CARDS_SUCCESS);
      parseAndShowNotificationCards(request.response, callback);
    } else {
      callback();
    }
  };

  setAuthorization(request, function(success) {
    if (success) {
      tasks.debugSetStepName('requestNotificationCards-send-request');
      request.send(requestParameters);
    } else {
      callback();
    }
  });
}

/**
 * Starts getting location for a cards update.
 */
function requestLocation() {
  console.log('requestLocation');
  recordEvent(DiagnosticEvent.LOCATION_REQUEST);
  // TODO(vadimt): Figure out location request options.
  chrome.location.watchLocation(LOCATION_WATCH_NAME, {});
}


/**
 * Obtains new location; requests and shows notification cards based on this
 * location.
 * @param {Location} position Location of this computer.
 */
function updateNotificationsCards(position) {
  console.log('updateNotificationsCards ' + JSON.stringify(position) +
      ' @' + new Date());
  tasks.add(UPDATE_CARDS_TASK_NAME, function(callback) {
    console.log('updateNotificationsCards-task-begin');
    updateCardsAttempts.planForNext(function() {
      processPendingDismissals(function(success) {
        if (success) {
          // The cards are requested only if there are no unsent dismissals.
          requestNotificationCards(position, callback);
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
 * @param {number} dismissalTimeMs Time of the user's dismissal of the card in
 *     milliseconds since epoch.
 * @param {function(boolean)} callbackBoolean Completion callback with 'success'
 *     parameter.
 */
function requestCardDismissal(
    notificationId, dismissalTimeMs, callbackBoolean) {
  console.log('requestDismissingCard ' + notificationId + ' from ' +
      NOTIFICATION_CARDS_URL);
  recordEvent(DiagnosticEvent.DISMISS_REQUEST_TOTAL);
  // Send a dismiss request to the server.
  var requestParameters = 'id=' + notificationId +
                          '&dismissalAge=' + (Date.now() - dismissalTimeMs);
  var request = buildServerRequest('dismiss');
  request.onloadend = function(event) {
    console.log('requestDismissingCard-onloadend ' + request.status);
    if (request.status == HTTP_OK)
      recordEvent(DiagnosticEvent.DISMISS_REQUEST_SUCCESS);

    callbackBoolean(request.status == HTTP_OK);
  };

  setAuthorization(request, function(success) {
    if (success) {
      tasks.debugSetStepName('requestCardDismissal-send-request');
      request.send(requestParameters);
    } else {
      callbackBoolean(false);
    }
  });
}

/**
 * Tries to send dismiss requests for all pending dismissals.
 * @param {function(boolean)} callbackBoolean Completion callback with 'success'
 *     parameter. Success means that no pending dismissals are left.
 */
function processPendingDismissals(callbackBoolean) {
  tasks.debugSetStepName('processPendingDismissals-storage-get');
  storage.get(['pendingDismissals', 'recentDismissals'], function(items) {
    console.log('processPendingDismissals-storage-get ' +
                JSON.stringify(items));
    items.pendingDismissals = items.pendingDismissals || [];
    items.recentDismissals = items.recentDismissals || {};

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
        dismissalAttempts.stop();
        onFinish(true);
        return;
      }

      // Send dismissal for the first card, and if successful, repeat
      // recursively with the rest.
      var dismissal = items.pendingDismissals[0];
      requestCardDismissal(
          dismissal.notificationId, dismissal.time, function(success) {
        if (success) {
          dismissalsChanged = true;
          items.pendingDismissals.splice(0, 1);
          items.recentDismissals[dismissal.notificationId] = Date.now();
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
    dismissalAttempts.planForNext(function() {
      processPendingDismissals(function(success) { callback(); });
     });
  });
}

/**
 * Opens URL corresponding to the clicked part of the notification.
 * @param {string} notificationId Unique identifier of the notification.
 * @param {function(Object): string} selector Function that extracts the url for
 *     the clicked area from the button action URLs info.
 */
function onNotificationClicked(notificationId, selector) {
  storage.get('notificationsData', function(items) {
    items.notificationsData = items.notificationsData || {};

    var notificationData = items.notificationsData[notificationId];

    if (!notificationData) {
      // 'notificationsData' in storage may not match the actual list of
      // notifications.
      return;
    }

    var actionUrls = notificationData.actionUrls;
    if (typeof actionUrls != 'object') {
      return;
    }

    var url = selector(actionUrls);

    if (typeof url != 'string')
      return;

    chrome.tabs.create({url: url}, function(tab) {
      if (!tab)
        chrome.windows.create({url: url});
    });
  });
}

/**
 * Responds to a click of one of the buttons on the welcome toast.
 * @param {number} buttonIndex The index of the button which was clicked.
 */
function onToastNotificationClicked(buttonIndex) {
  if (buttonIndex == ToastButtonIndex.YES) {
    chrome.metricsPrivate.recordUserAction('GoogleNow.WelcomeToastClickedYes');
    storage.set({toastState: ToastOptionResponse.CHOSE_YES});

    // TODO(zturner): Update chrome geolocation setting once the settings
    // API is in place.
    startPollingCards();
  } else {
    chrome.metricsPrivate.recordUserAction('GoogleNow.WelcomeToastClickedNo');
    storage.set({toastState: ToastOptionResponse.CHOSE_NO});
  }

  chrome.notifications.clear(
      WELCOME_TOAST_NOTIFICATION_ID,
      function(wasCleared) {});
}

/**
 * Callback for chrome.notifications.onClosed event.
 * @param {string} notificationId Unique identifier of the notification.
 * @param {boolean} byUser Whether the notification was closed by the user.
 */
function onNotificationClosed(notificationId, byUser) {
  if (!byUser)
    return;

  if (notificationId == WELCOME_TOAST_NOTIFICATION_ID) {
    // Even though they only closed the notification without clicking no, treat
    // it as though they clicked No anwyay, and don't show the toast again.
    chrome.metricsPrivate.recordUserAction('GoogleNow.WelcomeToastDismissed');
    storage.set({toastState: ToastOptionResponse.CHOSE_NO});
    return;
  }

  // At this point we are guaranteed that the notification is a now card.
  chrome.metricsPrivate.recordUserAction('GoogleNow.Dismissed');

  tasks.add(DISMISS_CARD_TASK_NAME, function(callback) {
    dismissalAttempts.start();

    // Deleting the notification in case it was re-added while this task was
    // scheduled, waiting for execution.
    chrome.notifications.clear(
        notificationId,
        function() {});

    tasks.debugSetStepName('onNotificationClosed-get-pendingDismissals');
    storage.get('pendingDismissals', function(items) {
      items.pendingDismissals = items.pendingDismissals || [];

      var dismissal = {
        notificationId: notificationId,
        time: Date.now()
      };
      items.pendingDismissals.push(dismissal);
      storage.set({pendingDismissals: items.pendingDismissals});
      processPendingDismissals(function(success) { callback(); });
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
 * Initializes the event page on install or on browser startup.
 */
function initialize() {
  storage.get('toastState', function(items) {
    // The toast state might be undefined (e.g. not in storage yet) if this is
    // the first time ever being prompted.

    // TODO(zturner): Get the value of isGeolocationEnabled from the settings
    // api and additionally make sure it is true.
    if (!items.toastState) {
      showWelcomeToast();
    } else if (items.toastState == ToastOptionResponse.CHOSE_YES) {
      startPollingCards();
    }
  });
}

/**
 * Displays a toast to the user asking if they want to opt in to receiving
 * Google Now cards.
 */
function showWelcomeToast() {
  // TODO(zturner): Localize this once the component extension localization
  // api is complete.
  // TODO(zturner): Add icons.
  var buttons = [{title: 'Yes'}, {title: 'No'}];
  var options = {
    type: 'basic',
    title: 'Enable Google Now Cards',
    message: 'Would you like to be shown Google Now cards?',
    iconUrl: 'http://www.gstatic.com/googlenow/chrome/default.png',
    priority: 2,
    buttons: buttons
  };
  chrome.notifications.create(WELCOME_TOAST_NOTIFICATION_ID, options,
      function(notificationId) {});
}

chrome.runtime.onInstalled.addListener(function(details) {
  console.log('onInstalled ' + JSON.stringify(details));
  if (details.reason != 'chrome_update') {
    initialize();
  }
});

chrome.runtime.onStartup.addListener(function() {
  console.log('onStartup');
  initialize();
});

chrome.notifications.onClicked.addListener(
    function(notificationId) {
      chrome.metricsPrivate.recordUserAction('GoogleNow.MessageClicked');
      onNotificationClicked(notificationId, function(actionUrls) {
        return actionUrls.messageUrl;
      });
    });

chrome.notifications.onButtonClicked.addListener(
    function(notificationId, buttonIndex) {
      if (notificationId == WELCOME_TOAST_NOTIFICATION_ID) {
        onToastNotificationClicked(buttonIndex);
      } else {
        chrome.metricsPrivate.recordUserAction(
            'GoogleNow.ButtonClicked' + buttonIndex);
        onNotificationClicked(notificationId, function(actionUrls) {
          if (!Array.isArray(actionUrls.buttonUrls))
            return undefined;

          return actionUrls.buttonUrls[buttonIndex];
        });
      }
    });

chrome.notifications.onClosed.addListener(onNotificationClosed);

chrome.location.onLocationUpdate.addListener(function(position) {
  recordEvent(DiagnosticEvent.LOCATION_UPDATE);
  updateNotificationsCards(position);
});

chrome.omnibox.onInputEntered.addListener(function(text) {
  localStorage['server_url'] = NOTIFICATION_CARDS_URL = text;
  initialize();
});
