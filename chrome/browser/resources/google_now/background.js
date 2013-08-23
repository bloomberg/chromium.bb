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
// TODO(vadimt): Consider sending JS stacks for malformed server responses.

/**
 * Standard response code for successful HTTP requests. This is the only success
 * code the server will send.
 */
var HTTP_OK = 200;

var HTTP_BAD_REQUEST = 400;
var HTTP_UNAUTHORIZED = 401;
var HTTP_FORBIDDEN = 403;
var HTTP_METHOD_NOT_ALLOWED = 405;

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
 * Time we keep retrying dismissals.
 */
var MAXIMUM_DISMISSAL_AGE_MS = 24 * 60 * 60 * 1000; // 1 day

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
var STATE_CHANGED_TASK_NAME = 'state-changed';

var LOCATION_WATCH_NAME = 'location-watch';

var WELCOME_TOAST_NOTIFICATION_ID = 'enable-now-toast';

/**
 * The indices of the buttons that are displayed on the welcome toast.
 * @enum {number}
 */
var ToastButtonIndex = {YES: 0, NO: 1};

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
tasks.instrumentChromeApiFunction('location.onLocationUpdate.addListener', 0);
tasks.instrumentChromeApiFunction('metricsPrivate.getVariationParams', 1);
tasks.instrumentChromeApiFunction('notifications.create', 2);
tasks.instrumentChromeApiFunction('notifications.update', 2);
tasks.instrumentChromeApiFunction('notifications.getAll', 0);
tasks.instrumentChromeApiFunction(
    'notifications.onButtonClicked.addListener', 0);
tasks.instrumentChromeApiFunction('notifications.onClicked.addListener', 0);
tasks.instrumentChromeApiFunction('notifications.onClosed.addListener', 0);
tasks.instrumentChromeApiFunction('omnibox.onInputEntered.addListener', 0);
tasks.instrumentChromeApiFunction(
    'preferencesPrivate.googleGeolocationAccessEnabled.get',
    1);
tasks.instrumentChromeApiFunction(
    'preferencesPrivate.googleGeolocationAccessEnabled.onChange.addListener',
    0);
tasks.instrumentChromeApiFunction('permissions.contains', 1);
tasks.instrumentChromeApiFunction('permissions.remove', 1);
tasks.instrumentChromeApiFunction('permissions.request', 1);
tasks.instrumentChromeApiFunction('runtime.onInstalled.addListener', 0);
tasks.instrumentChromeApiFunction('runtime.onStartup.addListener', 0);
tasks.instrumentChromeApiFunction('tabs.create', 1);
tasks.instrumentChromeApiFunction('storage.local.get', 1);

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
  SHOW_WELCOME_TOAST: 8,
  STOPPED: 9,
  USER_SUPPRESSED: 10,
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
  tasks.debugSetStepName('setAuthorization-isSignedIn');
  authenticationManager.isSignedIn(function(token) {
    if (!token) {
      callbackBoolean(false);
      return;
    }

    request.setRequestHeader('Authorization', 'Bearer ' + token);

    // Instrument onloadend to remove stale auth tokens.
    var originalOnLoadEnd = request.onloadend;
    request.onloadend = tasks.wrapCallback(function(event) {
      if (request.status == HTTP_FORBIDDEN ||
          request.status == HTTP_UNAUTHORIZED) {
        tasks.debugSetStepName('setAuthorization-removeToken');
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

  if (typeof parsedResponse.next_poll_seconds != 'number') {
    callback();
    return;
  }

  tasks.debugSetStepName('parseAndShowNotificationCards-storage-get');
  instrumented.storage.local.get(['notificationsData', 'recentDismissals'],
      function(items) {
        console.log('parseAndShowNotificationCards-get ' +
            JSON.stringify(items));
        items.notificationsData = items.notificationsData || {};
        items.recentDismissals = items.recentDismissals || {};

        tasks.debugSetStepName(
            'parseAndShowNotificationCards-notifications-getAll');
        instrumented.notifications.getAll(function(notifications) {
          console.log('parseAndShowNotificationCards-getAll ' +
              JSON.stringify(notifications));
          // TODO(vadimt): Figure out what to do when notifications are
          // disabled for our extension.
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
              console.log('parseAndShowNotificationCards-delete ' +
                  notificationId);
              cardSet.clear(notificationId);
            }
          }

          recordEvent(GoogleNowEvent.CARDS_PARSE_SUCCESS);

          // Create/update notifications and store their new properties.
          var newNotificationsData = {};
          for (var i = 0; i < cards.length; ++i) {
            var card = cards[i];
            if (!(card.notificationId in updatedRecentDismissals)) {
              var notificationData =
                  items.notificationsData[card.notificationId];
              var previousVersion = notifications[card.notificationId] &&
                                    notificationData &&
                                    notificationData.cardCreateInfo &&
                                    notificationData.cardCreateInfo.version;
              newNotificationsData[card.notificationId] =
                  cardSet.update(card, previousVersion);
            }
          }

          updateCardsAttempts.start(parsedResponse.next_poll_seconds);

          chrome.storage.local.set({
            notificationsData: newNotificationsData,
            recentDismissals: updatedRecentDismissals
          });
          callback();
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
    for (var notificationId in notifications) {
      chrome.notifications.clear(notificationId, function() {});
    }
    chrome.storage.local.set({notificationsData: {}});
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

  recordEvent(GoogleNowEvent.REQUEST_FOR_CARDS_TOTAL);

  // TODO(vadimt): Should we use 'q' as the parameter name?
  var requestParameters =
      'q=' + position.coords.latitude +
      ',' + position.coords.longitude +
      ',' + position.coords.accuracy;

  var request = buildServerRequest('notifications',
                                   'application/x-www-form-urlencoded');

  request.onloadend = function(event) {
    console.log('requestNotificationCards-onloadend ' + request.status);
    if (request.status == HTTP_OK) {
      recordEvent(GoogleNowEvent.REQUEST_FOR_CARDS_SUCCESS);
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
  recordEvent(GoogleNowEvent.LOCATION_REQUEST);
  // TODO(vadimt): Figure out location request options.
  instrumented.metricsPrivate.getVariationParams('GoogleNow', function(params) {
    var minDistanceInMeters =
        parseInt(params && params.minDistanceInMeters, 10) ||
        100;
    var minTimeInMilliseconds =
        parseInt(params && params.minTimeInMilliseconds, 10) ||
        180000;  // 3 minutes.

    chrome.location.watchLocation(LOCATION_WATCH_NAME, {
      minDistanceInMeters: minDistanceInMeters,
      minTimeInMilliseconds: minTimeInMilliseconds
    });
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
  tasks.add(UPDATE_CARDS_TASK_NAME, function(callback) {
    console.log('updateNotificationsCards-task-begin');
    updateCardsAttempts.isRunning(function(running) {
      if (running) {
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
      }
    });
  });
}

/**
 * Sends a server request to dismiss a card.
 * @param {string} notificationId Unique identifier of the card.
 * @param {number} dismissalTimeMs Time of the user's dismissal of the card in
 *     milliseconds since epoch.
 * @param {Object} dismissalParameters Dismissal parameters.
 * @param {function(boolean)} callbackBoolean Completion callback with 'done'
 *     parameter.
 */
function requestCardDismissal(
    notificationId, dismissalTimeMs, dismissalParameters, callbackBoolean) {
  console.log('requestDismissingCard ' + notificationId + ' from ' +
      NOTIFICATION_CARDS_URL);

  var dismissalAge = Date.now() - dismissalTimeMs;

  if (dismissalAge > MAXIMUM_DISMISSAL_AGE_MS) {
    callbackBoolean(true);
    return;
  }

  recordEvent(GoogleNowEvent.DISMISS_REQUEST_TOTAL);
  var request = buildServerRequest('dismiss', 'application/json');
  request.onloadend = function(event) {
    console.log('requestDismissingCard-onloadend ' + request.status);
    if (request.status == HTTP_OK)
      recordEvent(GoogleNowEvent.DISMISS_REQUEST_SUCCESS);

    // A dismissal doesn't require further retries if it was successful or
    // doesn't have a chance for successful completion.
    var done = request.status == HTTP_OK ||
        request.status == HTTP_BAD_REQUEST ||
        request.status == HTTP_METHOD_NOT_ALLOWED;
    callbackBoolean(done);
  };

  setAuthorization(request, function(success) {
    if (success) {
      tasks.debugSetStepName('requestCardDismissal-send-request');

      var dismissalObject = {
        id: notificationId,
        age: dismissalAge,
        dismissal: dismissalParameters
      };
      request.send(JSON.stringify(dismissalObject));
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
  instrumented.storage.local.get(['pendingDismissals', 'recentDismissals'],
      function(items) {
        console.log('processPendingDismissals-storage-get ' +
                    JSON.stringify(items));
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
              dismissal.notificationId,
              dismissal.time,
              dismissal.parameters,
              function(done) {
                if (done) {
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
  instrumented.storage.local.get('notificationsData', function(items) {
    var notificationData = items &&
        items.notificationsData &&
        items.notificationsData[notificationId];

    if (!notificationData)
      return;

    var actionUrls = notificationData.actionUrls;
    if (typeof actionUrls != 'object') {
      return;
    }

    var url = selector(actionUrls);
    if (!url)
      return;

    instrumented.tabs.create({url: url}, function(tab) {
      if (tab)
        chrome.windows.update(tab.windowId, {focused: true});
      else
        chrome.windows.create({url: url, focused: true});
    });
  });
}

/**
 * Responds to a click of one of the buttons on the welcome toast.
 * @param {number} buttonIndex The index of the button which was clicked.
 */
function onToastNotificationClicked(buttonIndex) {
  chrome.storage.local.set({userRespondedToToast: true});

  if (buttonIndex == ToastButtonIndex.YES) {
    chrome.metricsPrivate.recordUserAction('GoogleNow.WelcomeToastClickedYes');
    chrome.preferencesPrivate.googleGeolocationAccessEnabled.set({value: true});
    // The googlegeolocationaccessenabled preference change callback
    // will take care of starting the poll for cards.
  } else {
    chrome.metricsPrivate.recordUserAction('GoogleNow.WelcomeToastClickedNo');
    onStateChange();
  }
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
    chrome.storage.local.set({userRespondedToToast: true});
    return;
  }

  // At this point we are guaranteed that the notification is a now card.
  chrome.metricsPrivate.recordUserAction('GoogleNow.Dismissed');

  tasks.add(DISMISS_CARD_TASK_NAME, function(callback) {
    dismissalAttempts.start();

    // Deleting the notification in case it was re-added while this task was
    // scheduled, waiting for execution.
    cardSet.clear(notificationId);

    tasks.debugSetStepName('onNotificationClosed-storage-get');
    instrumented.storage.local.get(['pendingDismissals', 'notificationsData'],
        function(items) {
          items.pendingDismissals = items.pendingDismissals || [];
          items.notificationsData = items.notificationsData || {};

          var notificationData = items.notificationsData[notificationId];

          var dismissal = {
            notificationId: notificationId,
            time: Date.now(),
            parameters: notificationData && notificationData.dismissalParameters
          };
          items.pendingDismissals.push(dismissal);
          chrome.storage.local.set(
              {pendingDismissals: items.pendingDismissals});
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
 * Stops all machinery in the polling system.
 */
function stopPollingCards() {
  stopRequestLocation();

  updateCardsAttempts.stop();

  removeAllCards();
}

/**
 * Initializes the event page on install or on browser startup.
 */
function initialize() {
  recordEvent(GoogleNowEvent.EXTENSION_START);

  // Alarms persist across chrome restarts. This is undesirable since it
  // prevents us from starting up everything (alarms are a heuristic to
  // determine if we are already running). To mitigate this, we will
  // shut everything down on initialize before starting everything up.
  stopPollingCards();
  onStateChange();
}

/**
 * Starts or stops the polling of cards.
 * @param {boolean} shouldPollCardsRequest true to start and
 *     false to stop polling cards.
 * @param {function} callback Called on completion.
 */
function setShouldPollCards(shouldPollCardsRequest, callback) {
  tasks.debugSetStepName(
        'setShouldRun-shouldRun-updateCardsAttemptsIsRunning');
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
    callback();
  });
}

/**
 * Shows or hides the toast.
 * @param {boolean} visibleRequest true to show the toast and
 *     false to hide the toast.
 * @param {function} callback Called on completion.
 */
function setToastVisible(visibleRequest, callback) {
  tasks.debugSetStepName(
      'setToastVisible-shouldSetToastVisible-getAllNotifications');
  instrumented.notifications.getAll(function(notifications) {
    // TODO(vadimt): Figure out what to do when notifications are disabled for
    // our extension.
    notifications = notifications || {};

    if (visibleRequest != !!notifications[WELCOME_TOAST_NOTIFICATION_ID]) {
      console.log('Action Taken setToastVisible=' + visibleRequest);
      if (visibleRequest)
        showWelcomeToast();
      else
        hideWelcomeToast();
    } else {
      console.log('Action Ignored setToastVisible=' + visibleRequest);
    }

    callback();
  });
}

/**
 * Enables or disables the Google Now background permission.
 * @param {boolean} backgroundEnable true to run in the background.
 *     false to not run in the background.
 * @param {function} callback Called on completion.
 */
function setBackgroundEnable(backgroundEnable, callback) {
  instrumented.permissions.contains({permissions: ['background']},
      function(hasPermission) {
        if (backgroundEnable != hasPermission) {
          console.log('Action Taken setBackgroundEnable=' + backgroundEnable);
          if (backgroundEnable)
            instrumented.permissions.request(
                {permissions: ['background']},
                function() {
                  callback();
                });
          else
            instrumented.permissions.remove(
                {permissions: ['background']},
                function() {
                  callback();
                });
        } else {
          console.log('Action Ignored setBackgroundEnable=' + backgroundEnable);
          callback();
        }
      });
}

/**
 * Does the actual work of deciding what Google Now should do
 * based off of the current state of Chrome.
 * @param {boolean} signedIn true if the user is signed in.
 * @param {boolean} geolocationEnabled true if
 *     the geolocation option is enabled.
 * @param {boolean} userRespondedToToast true if
 *     the user has responded to the toast.
 * @param {boolean} enableBackground true if
 *     the background permission should be requested.
 * @param {function()} callback Call this function on completion.
 */
function updateRunningState(
    signedIn,
    geolocationEnabled,
    userRespondedToToast,
    enableBackground,
    callback) {

  console.log(
      'State Update signedIn=' + signedIn + ' ' +
      'geolocationEnabled=' + geolocationEnabled + ' ' +
      'userRespondedToToast=' + userRespondedToToast);

  var shouldSetToastVisible = false;
  var shouldPollCards = false;
  var shouldSetBackground = false;

  if (signedIn) {
    if (geolocationEnabled) {
      if (!userRespondedToToast) {
        // If the user enabled geolocation independently of Google Now,
        // the user has implicitly responded to the toast.
        // We do not want to show it again.
        chrome.storage.local.set({userRespondedToToast: true});
      }

      if (enableBackground)
        shouldSetBackground = true;

      shouldPollCards = true;
    } else {
      if (userRespondedToToast) {
        recordEvent(GoogleNowEvent.USER_SUPPRESSED);
      } else {
        shouldSetToastVisible = true;
      }
    }
  } else {
    recordEvent(GoogleNowEvent.STOPPED);
  }

  console.log(
      'Requested Actions shouldSetBackground=' + shouldSetBackground + ' ' +
      'setToastVisible=' + shouldSetToastVisible + ' ' +
      'setShouldPollCards=' + shouldPollCards);

  setBackgroundEnable(shouldSetBackground, function() {
    setToastVisible(shouldSetToastVisible, function() {
      setShouldPollCards(shouldPollCards, callback);
    });
  });
}

/**
 * Coordinates the behavior of Google Now for Chrome depending on
 * Chrome and extension state.
 */
function onStateChange() {
  tasks.add(STATE_CHANGED_TASK_NAME, function(callback) {
    tasks.debugSetStepName('onStateChange-isSignedIn');
    authenticationManager.isSignedIn(function(token) {
      var signedIn = !!token && !!NOTIFICATION_CARDS_URL;
      instrumented.metricsPrivate.getVariationParams(
          'GoogleNow',
          function(response) {
            var enableBackground =
                (!response || (response.enableBackground != 'false'));
            tasks.debugSetStepName(
                'onStateChange-get-googleGeolocationAccessEnabledPref');
            instrumented.
                preferencesPrivate.
                googleGeolocationAccessEnabled.
                get({}, function(prefValue) {
                  var geolocationEnabled = !!prefValue.value;
                  tasks.debugSetStepName(
                    'onStateChange-get-userRespondedToToast');
                  instrumented.storage.local.get(
                      'userRespondedToToast',
                      function(items) {
                        var userRespondedToToast =
                            !items || !!items.userRespondedToToast;
                        updateRunningState(
                            signedIn,
                            geolocationEnabled,
                            userRespondedToToast,
                            enableBackground,
                            callback);
                      });
                });
          });
    });
  });
}

/**
 * Displays a toast to the user asking if they want to opt in to receiving
 * Google Now cards.
 */
function showWelcomeToast() {
  recordEvent(GoogleNowEvent.SHOW_WELCOME_TOAST);
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
  instrumented.notifications.create(WELCOME_TOAST_NOTIFICATION_ID, options,
      function(notificationId) {});
}

/**
 * Hides the welcome toast.
 */
function hideWelcomeToast() {
  chrome.notifications.clear(
      WELCOME_TOAST_NOTIFICATION_ID,
      function() {});
}

instrumented.runtime.onInstalled.addListener(function(details) {
  console.log('onInstalled ' + JSON.stringify(details));
  if (details.reason != 'chrome_update') {
    initialize();
  }
});

instrumented.runtime.onStartup.addListener(function() {
  console.log('onStartup');
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
    function(notificationId) {
      chrome.metricsPrivate.recordUserAction('GoogleNow.MessageClicked');
      onNotificationClicked(notificationId, function(actionUrls) {
        return actionUrls.messageUrl;
      });
    });

instrumented.notifications.onButtonClicked.addListener(
    function(notificationId, buttonIndex) {
      if (notificationId == WELCOME_TOAST_NOTIFICATION_ID) {
        onToastNotificationClicked(buttonIndex);
      } else {
        chrome.metricsPrivate.recordUserAction(
            'GoogleNow.ButtonClicked' + buttonIndex);
        onNotificationClicked(notificationId, function(actionUrls) {
          var url = actionUrls.buttonUrls[buttonIndex];
          verify(url, 'onButtonClicked: no url for a button');
          return url;
        });
      }
    });

instrumented.notifications.onClosed.addListener(onNotificationClosed);

instrumented.location.onLocationUpdate.addListener(function(position) {
  recordEvent(GoogleNowEvent.LOCATION_UPDATE);
  updateNotificationsCards(position);
});

instrumented.omnibox.onInputEntered.addListener(function(text) {
  localStorage['server_url'] = NOTIFICATION_CARDS_URL = text;
  initialize();
});
