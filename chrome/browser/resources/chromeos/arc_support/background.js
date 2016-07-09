// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * UI Pages. Note the order must be in sync with the ArcAuthService::UIPage
 * enum.
 * @type {Array<string>}
 */
var UI_PAGES = ['none',
                'terms-loading',
                'terms',
                'lso-loading',
                'lso',
                'arc-loading',
                'error',
                'error-with-feedback'];

/**
 * Chrome window that hosts UI. Only one window is allowed.
 * @type {chrome.app.window.AppWindow}
 */
var appWindow = null;

/**
 * Contains Web content provided by Google authorization server.
 * @type {WebView}
 */
var lsoView = null;

/**
 * Contains Play Store terms, loaded externally.
 * @type {WebView}
 */
var termsView = null;

/**
 * Used for bidirectional communication with native code.
 * @type {chrome.runtime.Port}
 */
var port = null;

/**
 * Indicates that window was closed internally and it is not required to send
 * closure notification.
 * @type {boolean}
 */
var windowClosedInternally = false;

/**
 * Timer for terms reload.
 * @type {Object}
 */
var termsReloadTimeout = null;

/**
 * Stores current device id.
 * @type {string}
 */
var currentDeviceId = null;

/**
 * Indicates that terms were accepted by user.
 * @type {boolean}
 */
var termsAccepted = false;

/**
 * Closes current window in response to request from native code. This does not
 * issue 'cancelAuthCode' message to native code.
 */
function closeWindowInternally() {
  windowClosedInternally = true;
  appWindow.close();
  appWindow = null;
}

/**
 * Sends a native message to ArcSupportHost.
 * @param {string} code The action code in message.
 * @param {Object=} opt_Props Extra properties for the message.
 */
function sendNativeMessage(code, opt_Props) {
  var message = Object.assign({'action': code}, opt_Props);
  port.postMessage(message);
}

/**
 * Applies localization for html content and sets terms webview.
 * @param {!Object} data Localized strings and relevant information.
 * @param {string} deviceId Current device id.
 */
function initialize(data, deviceId) {
  currentDeviceId = deviceId;
  var doc = appWindow.contentWindow.document;
  var loadTimeData = appWindow.contentWindow.loadTimeData;
  loadTimeData.data = data;
  appWindow.contentWindow.i18nTemplate.process(doc, loadTimeData);
  var countryCode = data.countryCode.toLowerCase();
  setBackupRestoreMode(data.textBackupRestore, data.backupRestoreEnabled);

  var scriptSetCountryCode = 'document.countryCode = \'' + countryCode + '\';';
  termsView.addContentScripts([
      { name: 'preProcess',
        matches: ['https://play.google.com/*'],
        js: { code: scriptSetCountryCode },
        run_at: 'document_start'
      },
      { name: 'postProcess',
        matches: ['https://play.google.com/*'],
        css: { files: ['playstore.css'] },
        js: { files: ['playstore.js'] },
        run_at: 'document_end'
      }]);
}

/**
 * Handles the event when the user clicks on a learn more link. Opens the
 * support page for the user.
 * @param {Event} event
 */
var onLearnMore = function(event) {
  var url = 'https://support.google.com/chromebook?p=playapps';
  chrome.browser.openTab({'url': url}, function() {});
  event.preventDefault();
};

/**
 * Sets current metrics mode.
 * @param {string} text Describes current metrics state.
 * @param {boolean} canEnable Defines if user is allowed to change this metrics
 *                            option.
 * @param {boolean} on Defines if metrics are active currently.
 */
function setMetricsMode(text, canEnable, on) {
  var doc = appWindow.contentWindow.document;
  var enableMetrics = doc.getElementById('enable-metrics');
  enableMetrics.hidden = !canEnable;
  enableMetrics.checked = on;

  var onSettings = function(event) {
    chrome.browser.openTab({'url': 'chrome://settings'}, function() {});
    event.preventDefault();
  };

  doc.getElementById('text-metrics').innerHTML = text;
  doc.getElementById('settings-link').addEventListener('click', onSettings);
  doc.getElementById('learn-more-link-metrics').addEventListener('click',
      onLearnMore);

  // Applying metrics mode changes page layout, update terms height.
  updateTermsHeight();
}

/**
 * Sets current metrics mode.
 * @param {string} text String used to display next to checkbox.
 * @param {boolean} defaultCheckValue Defines the default value for backup and
 *     restore checkbox.
 */
function setBackupRestoreMode(text, defaultCheckValue) {
  var doc = appWindow.contentWindow.document;
  doc.getElementById('enable-backup-restore').checked = defaultCheckValue;

  doc.getElementById('text-backup-restore').innerHTML = text;
  doc.getElementById('learn-more-link-backup-restore').addEventListener('click',
      onLearnMore);
}

/**
 * Updates terms view height manually because webview is not automatically
 * resized in case parent div element gets resized.
 */
function updateTermsHeight() {
  var setTermsHeight = function() {
    var doc = appWindow.contentWindow.document;
    var termsContainer = doc.getElementById('terms-container');
    // Reset terms-view height in order to stabilize style computation. For
    // some reason, child webview affects final result.
    termsView.style.height = '0px';
    var style = window.getComputedStyle(termsContainer, null);
    var height = style.getPropertyValue('height');
    termsView.style.height = height;
  };
  setTimeout(setTermsHeight, 0);
}

/**
 * Handles native messages received from ArcSupportHost.
 * @param {!Object} message The message received.
 */
function onNativeMessage(message) {
  if (!message.action) {
    return;
  }

  if (!appWindow) {
    console.warn('Received native message when window is not available.');
    return;
  }

  if (message.action == 'initialize') {
    initialize(message.data, message.deviceId);
  } else if (message.action == 'setMetricsMode') {
    setMetricsMode(message.text, message.canEnable, message.on);
  } else if (message.action == 'closeUI') {
    closeWindowInternally();
  } else if (message.action == 'showPage') {
    showPageWithStatus(message.page, message.status);
  }
}

/**
 * Connects to ArcSupportHost.
 */
function connectPort() {
  var hostName = 'com.google.arc_support';
  port = chrome.runtime.connectNative(hostName);
  port.onMessage.addListener(onNativeMessage);
}

/**
 * Shows requested page and hide others. Show appWindow if it was hidden before
 * for non 'none' pages. For 'none' page this closes host window.
 * @param {string} pageDivId id of divider of the page to show.
 */
function showPage(pageDivId) {
  if (!appWindow) {
    return;
  }

  if (pageDivId == 'none') {
    closeWindowInternally();
    return;
  }

  var doc = appWindow.contentWindow.document;
  var pages = doc.getElementsByClassName('section');
  var sendFeedbackElement = doc.getElementById('button-send-feedback');
  if (pageDivId == 'error-with-feedback') {
    // Only show feedback button if the pageDivId is 'error-with-feedback'.
    sendFeedbackElement.hidden = false;
    pageDivId = 'error';
  } else {
    sendFeedbackElement.hidden = true;
  }
  for (var i = 0; i < pages.length; i++) {
    pages[i].hidden = pages[i].id != pageDivId;
  }

  if (pageDivId == 'lso-loading') {
    lsoView.src = 'https://accounts.google.com/o/oauth2/v2/auth?client_id=' +
                  '1070009224336-sdh77n7uot3oc99ais00jmuft6sk2fg9.apps.' +
                  'googleusercontent.com&response_type=code&redirect_uri=oob&' +
                  'scope=https://www.google.com/accounts/OAuthLogin&' +
                  'device_type=arc_plus_plus&device_id=' + currentDeviceId +
                  '&hl=' + navigator.language;
  }
  appWindow.show();
}

/**
 * Sets error message.
 * @param {string} error message.
 */
function setErrorMessage(error) {
  if (!appWindow) {
    return;
  }
  var doc = appWindow.contentWindow.document;
  var messageElement = doc.getElementById('error-message');
  messageElement.innerText = error;
}

/**
 * Shows requested page.
 * @param {int} pageId Index of the page to show. Must be in the array range of
 * UI_PAGES.
 * @param {string} status associated with page string status, error message for
 *                        example.
 */
function showPageWithStatus(pageId, status) {
  if (!appWindow) {
    return;
  }

  if (UI_PAGES[pageId] == 'terms-loading') {
    termsAccepted = false;
    loadInitialTerms();
  } else {
    // Explicit request to start not from start page. Assume terms are
    // accepted in this case.
    termsAccepted = true;
  }

  if (UI_PAGES[pageId] == 'error' ||
      UI_PAGES[pageId] == 'error-with-feedback') {
    setErrorMessage(status);
  }
  showPage(UI_PAGES[pageId]);
}

/**
 * Loads initial Play Store terms.
 */
function loadInitialTerms() {
  termsView.src = 'https://play.google.com/about/play-terms.html';
}

chrome.app.runtime.onLaunched.addListener(function() {
  var onAppContentLoad = function() {
    var doc = appWindow.contentWindow.document;
    lsoView = doc.getElementById('arc-support');

    var isApprovalResponse = function(url) {
      var resultUrlPrefix = 'https://accounts.google.com/o/oauth2/approval?';
      return url.substring(0, resultUrlPrefix.length) == resultUrlPrefix;
    };

    var lsoError = false;
    var onLsoViewRequestResponseStarted = function(details) {
      if (isApprovalResponse(details.url)) {
        showPage('arc-loading');
      }
      lsoError = false;
    };

    var onLsoViewErrorOccurred = function(details) {
      setErrorMessage(appWindow.contentWindow.loadTimeData.getString(
          'serverError'));
      showPage('error');
      lsoError = true;
    };

    var onLsoViewContentLoad = function() {
      if (lsoError) {
        return;
      }

      if (!isApprovalResponse(lsoView.src)) {
        // Show LSO page when its content is ready.
        showPage('lso');
        return;
      }

      lsoView.executeScript({code: 'document.title;'}, function(results) {
        var authCodePrefix = 'Success code=';
        if (results && results.length == 1 && typeof results[0] == 'string' &&
            results[0].substring(0, authCodePrefix.length) == authCodePrefix) {
          var authCode = results[0].substring(authCodePrefix.length);
          sendNativeMessage('setAuthCode', {code: authCode});
        } else {
          setErrorMessage(appWindow.contentWindow.loadTimeData.getString(
              'authorizationFailed'));
          showPage('error');
        }
      });
    };

    var requestFilter = {
      urls: ['<all_urls>'],
      types: ['main_frame']
    };

    lsoView.request.onResponseStarted.addListener(
        onLsoViewRequestResponseStarted, requestFilter);
    lsoView.request.onErrorOccurred.addListener(
        onLsoViewErrorOccurred, requestFilter);
    lsoView.addEventListener('contentload', onLsoViewContentLoad);

    termsView = doc.getElementById('terms-view');

    var termsError = false;
    var onTermsViewBeforeRequest = function(details) {
      showPage('terms-loading');
      termsError = false;
    };

    var onTermsViewErrorOccurred = function(details) {
      termsAccepted = false;
      setErrorMessage(appWindow.contentWindow.loadTimeData.getString(
          'serverError'));
      showPage('error');
      termsError = true;
    };

    var onTermsViewContentLoad = function() {
      if (termsError) {
        return;
      }
      showPage('terms');
      updateTermsHeight();
    };

    termsView.request.onBeforeRequest.addListener(onTermsViewBeforeRequest,
                                                  requestFilter);
    termsView.request.onErrorOccurred.addListener(onTermsViewErrorOccurred,
                                                  requestFilter);
    termsView.addEventListener('contentload', onTermsViewContentLoad);


    // webview is not allowed to open links in the new window. Hook these events
    // and open links in context of main page.
    termsView.addEventListener('newwindow', function(event) {
      event.preventDefault();
      chrome.browser.openTab({'url': event.targetUrl}, function() {});
    });

    var onAgree = function() {
      termsAccepted = true;

      var enableMetrics = doc.getElementById('enable-metrics');
      if (!enableMetrics.hidden) {
        sendNativeMessage('enableMetrics', {
          'enabled': enableMetrics.checked
        });
      }

      var enableBackupRestore = doc.getElementById('enable-backup-restore');
      sendNativeMessage('setBackupRestore', {
        'enabled': enableBackupRestore.checked
      });

      sendNativeMessage('startLso');
    };

    var onCancel = function() {
      if (appWindow) {
        windowClosedInternally = false;
        appWindow.close();
        appWindow = null;
      }
    };

    var onRetry = function() {
      if (termsAccepted) {
        sendNativeMessage('startLso');
      } else {
        loadInitialTerms();
      }
    };

    var onSendFeedback = function() {
      sendNativeMessage('sendFeedback');
    };

    doc.getElementById('button-agree').addEventListener('click', onAgree);
    doc.getElementById('button-cancel').addEventListener('click', onCancel);
    doc.getElementById('button-retry').addEventListener('click', onRetry);
    doc.getElementById('button-send-feedback')
        .addEventListener('click', onSendFeedback);

    connectPort();
  };

  var onWindowCreated = function(createdWindow) {
    appWindow = createdWindow;
    appWindow.contentWindow.onload = onAppContentLoad;
    createdWindow.onClosed.addListener(onWindowClosed);
  };

  var onWindowClosed = function() {
    if (termsReloadTimeout) {
      clearTimeout(termsReloadTimeout);
      termsReloadTimeout = null;
    }

    if (windowClosedInternally) {
      return;
    }
    sendNativeMessage('cancelAuthCode');
  };

  windowClosedInternally = false;
  var options = {
    'id': 'play_store_wnd',
    'resizable': false,
    'hidden': true,
    'frame': {
      type: 'chrome',
      color: '#ffffff'
    },
    'innerBounds': {
      'width': 960,
      'height': 688
    }
  };
  chrome.app.window.create('main.html', options, onWindowCreated);
});
