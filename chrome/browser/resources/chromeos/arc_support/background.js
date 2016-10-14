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
 * Indicates that current user has managed Arc.
 * @type {boolean}
 */
var arcManaged = false;

/**
 * Tooltip text used in 'controlled by policy' indicator.
 * @type {boolean}
 */
var controlledByPolicyText = '';

/**
 * Host window inner default width.
 * @const {number}
 */
var INNER_WIDTH = 960;

/**
 * Host window inner default height.
 * @const {number}
 */
var INNER_HEIGHT = 688;


/**
 * Sends a native message to ArcSupportHost.
 * @param {string} event The event type in message.
 * @param {Object=} opt_props Extra properties for the message.
 */
function sendNativeMessage(event, opt_props) {
  var message = Object.assign({'event': event}, opt_props);
  port.postMessage(message);
}

/**
 * Helper function that sets inner content for an option which includes text,
 * link to 'learn more' section. This also creates an indicator showing that
 * option is controlled by policy and inserts it before link element.
 * @param {string} textId Id of the label element to process.
 * @param {string} learnMoreLinkId Id inner link to 'learn more' element.
 * @param {string} indicatorId Id of indicator to create.
 * @param {string} text Inner text to set. Includes link declaration.
 * @param {function} callback Callback to call on user action.
 */
function createConsentOption(
    textId, learnMoreLinkId, indicatorId, text, callback) {
  var doc = appWindow.contentWindow.document;
  var textElement = doc.getElementById(textId);
  textElement.innerHTML = text;
  var linkLearnMoreElement = doc.getElementById(learnMoreLinkId);
  linkLearnMoreElement.addEventListener('click', callback);

  // Create controlled by policy indicator.
  var policyIndicator = new appWindow.contentWindow.cr.ui.ControlledIndicator();
  policyIndicator.id = indicatorId;
  policyIndicator.getBubbleText = function() {
    return controlledByPolicyText;
  };
  textElement.insertBefore(policyIndicator, linkLearnMoreElement);
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
  controlledByPolicyText = data.controlledByPolicy;
  arcManaged = data.arcManaged;
  setTermsVisible(!arcManaged);

  createConsentOption('text-backup-restore',
                      'learn-more-link-backup-restore',
                      'policy-indicator-backup-restore',
                      data.textBackupRestore,
                      onLearnMoreBackupAndRestore);
  createConsentOption('text-location-service',
                      'learn-more-link-location-service',
                      'policy-indicator-location-service',
                      data.textLocationService,
                      onLearnMoreLocationServices);

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
 * Handles the event when the user clicks on a learn more metrics link. Opens
 * the pop up dialog with a help.
 */
var onLearnMoreMetrics = function() {
  var loadTimeData = appWindow.contentWindow.loadTimeData;
  showLearnModeOverlay(loadTimeData.getString('learnMoreStatistics'));
};

/**
 * Handles the event when the user clicks on a learn more backup and restore
 * link. Opens the pop up dialog with a help.
 */
var onLearnMoreBackupAndRestore = function() {
  var loadTimeData = appWindow.contentWindow.loadTimeData;
  showLearnModeOverlay(loadTimeData.getString('learnMoreBackupAndRestore'));
};

/**
 * Handles the event when the user clicks on a learn more location services
 * link. Opens the pop up dialog with a help.
 */
var onLearnMoreLocationServices = function() {
  var loadTimeData = appWindow.contentWindow.loadTimeData;
  showLearnModeOverlay(loadTimeData.getString('learnMoreLocationServices'));
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
      onLearnMoreMetrics);

  // Applying metrics mode changes page layout, update terms height.
  updateTermsHeight();
}

/**
 * Sets current backup and restore mode.
 * @param {boolean} enabled Defines the value for backup and restore checkbox.
 * @param {boolean} managed Defines whether this setting is set by policy.
 */
function setBackupRestoreMode(enabled, managed) {
  var doc = appWindow.contentWindow.document;
  doc.getElementById('enable-backup-restore').checked = enabled;
  doc.getElementById('enable-backup-restore').disabled = managed;
  doc.getElementById('text-backup-restore').disabled = managed;
  var policyIconElement = doc.getElementById('policy-indicator-backup-restore');
  if (managed) {
    policyIconElement.setAttribute('controlled-by', 'policy');
  } else {
    policyIconElement.removeAttribute('controlled-by');
  }
}

/**
 * Sets current usage of location service opt in mode.
 * @param {boolean} enabled Defines the value for location service opt in.
 * @param {boolean} managed Defines whether this setting is set by policy.
 */
function setLocationServiceMode(enabled, managed) {
  var doc = appWindow.contentWindow.document;
  doc.getElementById('enable-location-service').checked = enabled;
  doc.getElementById('enable-location-service').disabled = managed;
  doc.getElementById('text-location-service').disabled = managed;
  var policyIconElement = doc.getElementById(
      'policy-indicator-location-service');
  if (managed) {
    policyIconElement.setAttribute('controlled-by', 'policy');
  } else {
    policyIconElement.removeAttribute('controlled-by');
  }
}

/**
 * Sets visibility of Terms of Service.
 * @param {boolean} visible Whether the Terms of Service visible or not.
 */
function setTermsVisible(visible) {
  var doc = appWindow.contentWindow.document;
  var styleVisibility = visible ? 'visible' : 'hidden';
  doc.getElementById('terms-title').style.visibility = styleVisibility;
  doc.getElementById('terms-container').style.visibility = styleVisibility;
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
  } else if (message.action == 'setBackupAndRestoreMode') {
    setBackupRestoreMode(message.enabled, message.managed);
  } else if (message.action == 'setLocationServiceMode') {
    setLocationServiceMode(message.enabled, message.managed);
  } else if (message.action == 'closeWindow') {
    if (appWindow) {
      appWindow.close();
    }
  } else if (message.action == 'showPage') {
    showPageWithStatus(message.page, message.status);
  } else if (message.action == 'setWindowBounds') {
    setWindowBounds();
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
 * Shows requested page and hide others. Show appWindow if it was hidden before.
 * 'none' hides all views.
 * @param {string} pageDivId id of divider of the page to show.
 */
function showPage(pageDivId) {
  if (!appWindow) {
    return;
  }

  hideLearnModeOverlay();
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
  if (pageDivId == 'terms') {
    updateTermsHeight();
  }
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
 * Sets learn more content text and shows it as overlay dialog.
 * @param {string} content HTML formatted text to show.
 */
function showLearnModeOverlay(content) {
  var doc = appWindow.contentWindow.document;
  var learnMoreContainer = doc.getElementById('learn-more-container');
  var learnMoreContent = doc.getElementById('learn-more-content');
  learnMoreContent.innerHTML = content;
  learnMoreContainer.hidden = false;
}

/**
 * Hides learn more overlay dialog.
 */
function hideLearnModeOverlay() {
  var doc = appWindow.contentWindow.document;
  var learnMoreContainer = doc.getElementById('learn-more-container');
  learnMoreContainer.hidden = true;
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
    termsAccepted = arcManaged;
    if (termsAccepted) {
      showPage('terms');
      return;
    }
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

function setWindowBounds() {
  if (!appWindow) {
    return;
  }

  var decorationWidth = appWindow.outerBounds.width -
      appWindow.innerBounds.width;
  var decorationHeight = appWindow.outerBounds.height -
      appWindow.innerBounds.height;

  var outerWidth = INNER_WIDTH + decorationWidth;
  var outerHeight = INNER_HEIGHT + decorationHeight;
  if (outerWidth > screen.availWidth) {
    outerWidth = screen.availWidth;
  }
  if (outerHeight > screen.availHeight) {
    outerHeight = screen.availHeight;
  }
  if (appWindow.outerBounds.width == outerWidth &&
      appWindow.outerBounds.height == outerHeight) {
    return;
  }

  appWindow.outerBounds.width = outerWidth;
  appWindow.outerBounds.height = outerHeight;
  appWindow.outerBounds.left = Math.ceil((screen.availWidth - outerWidth) / 2);
  appWindow.outerBounds.top =
    Math.ceil((screen.availHeight - outerHeight) / 2);
}

chrome.app.runtime.onLaunched.addListener(function() {
  var onAppContentLoad = function() {
    var doc = appWindow.contentWindow.document;
    lsoView = doc.getElementById('arc-support');
    lsoView.addContentScripts([
        { name: 'postProcess',
          matches: ['https://accounts.google.com/*'],
          css: { files: ['lso.css'] },
          run_at: 'document_end'
        }]);

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
        // We have fixed width for LSO page in css file in order to prevent
        // unwanted webview resize animation when it is shown first time. Now
        // it safe to make it up to window width.
        lsoView.style.width = '100%';
        return;
      }

      lsoView.executeScript({code: 'document.title;'}, function(results) {
        var authCodePrefix = 'Success code=';
        if (results && results.length == 1 && typeof results[0] == 'string' &&
            results[0].substring(0, authCodePrefix.length) == authCodePrefix) {
          var authCode = results[0].substring(authCodePrefix.length);
          sendNativeMessage('onAuthSucceeded', {code: authCode});
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
      var enableBackupRestore = doc.getElementById('enable-backup-restore');
      var enableLocationService = doc.getElementById('enable-location-service');
      sendNativeMessage('onAgreed', {
        isMetricsEnabled: enableMetrics.checked,
        isBackupRestoreEnabled: enableBackupRestore.checked,
        isLocationServiceEnabled: enableLocationService.checked
      });
    };

    var onCancel = function() {
      if (appWindow) {
        appWindow.close();
      }
    };

    var onRetry = function() {
      if (termsAccepted) {
        // Reuse the onAgree() in case that the user has already accepted
        // the ToS.
        onAgree();
      } else {
        loadInitialTerms();
      }
    };

    var onSendFeedback = function() {
      sendNativeMessage('onSendFeedbackClicked');
    };

    doc.getElementById('button-agree').addEventListener('click', onAgree);
    doc.getElementById('button-cancel').addEventListener('click', onCancel);
    doc.getElementById('button-retry').addEventListener('click', onRetry);
    doc.getElementById('button-send-feedback')
        .addEventListener('click', onSendFeedback);
    doc.getElementById('learn-more-close').addEventListener('click',
        hideLearnModeOverlay);

    var overlay = doc.getElementById('learn-more-container');
    appWindow.contentWindow.cr.ui.overlay.setupOverlay(overlay);
    appWindow.contentWindow.cr.ui.overlay.globalInitialization();
    overlay.addEventListener('cancelOverlay', hideLearnModeOverlay);

    connectPort();
  };

  var onWindowCreated = function(createdWindow) {
    appWindow = createdWindow;
    appWindow.contentWindow.onload = onAppContentLoad;
    appWindow.onClosed.addListener(onWindowClosed);

    setWindowBounds();
  };

  var onWindowClosed = function() {
    appWindow = null;

    // Notify to Chrome.
    sendNativeMessage('onWindowClosed');

    // On window closed, then dispose the extension. So, close the port
    // otherwise the background page would be kept alive so that the extension
    // would not be unloaded.
    port.disconnect();
    port = null;
  };

  var options = {
    'id': 'play_store_wnd',
    'resizable': false,
    'hidden': true,
    'frame': {
      type: 'chrome',
      color: '#ffffff'
    },
    'innerBounds': {
      'width': INNER_WIDTH,
      'height': INNER_HEIGHT
    }
  };
  chrome.app.window.create('main.html', options, onWindowCreated);
});
